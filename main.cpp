/*
 * Laboratorio 7 Microprocesadores
 * Angel Gabriel Chavez Otzoy - 24248
 * 16 / 09 / 2025
 */

#include <iostream>
#include <string>
#include <pthread.h>
#include <vector>
#include <fstream>
#include <zlib.h>
#include <chrono>
#include <cstring>

using namespace std;
using namespace std::chrono;

const size_t BLOCK_SIZE = 1024 * 1024;

struct CompressionData {
    int index;
    vector<char> originalData;
    vector<Bytef> compressedData;
    size_t compressedSize;
    bool success;
};

struct DecompressionData {
    int index;
    vector<Bytef> compressedData;
    vector<char> decompressedData;
    size_t originalSize;
    bool success;
};

// sincronizacion globales para sincronizacion
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;

void* compress_block(void* arg) {
    CompressionData* data = static_cast<CompressionData*>(arg);

    // tamaño maximo necesario para compresion
    uLongf compressedSize = compressBound(data->originalData.size());
    data->compressedData.resize(compressedSize);

    // comprimir el bloque
    int result = compress(data->compressedData.data(), &compressedSize,
                         reinterpret_cast<const Bytef*>(data->originalData.data()),
                         data->originalData.size());

    if (result == Z_OK) {
        data->compressedData.resize(compressedSize);
        data->compressedSize = compressedSize;
        data->success = true;

        pthread_mutex_lock(&console_mutex);
        cout << "Bloque " << data->index << " comprimido exitosamente. "
             << "Original: " << data->originalData.size()
             << " -> Comprimido: " << compressedSize << " bytes" << endl;
        pthread_mutex_unlock(&console_mutex);
    } else {
        data->success = false;
        pthread_mutex_lock(&console_mutex);
        cerr << "Error al comprimir bloque " << data->index
             << " (codigo " << result << ")" << endl;
        pthread_mutex_unlock(&console_mutex);
    }

    pthread_exit(nullptr);
}

void* decompress_block(void* arg) {
    DecompressionData* data = static_cast<DecompressionData*>(arg);

    // descomprimir con diferentes tamaños
    uLongf decompressedSize = data->originalSize;
    data->decompressedData.resize(decompressedSize);

    int result = uncompress(reinterpret_cast<Bytef*>(data->decompressedData.data()),
                           &decompressedSize,
                           data->compressedData.data(),
                           data->compressedData.size());

    if (result == Z_OK) {
        data->decompressedData.resize(decompressedSize);
        data->success = true;

        pthread_mutex_lock(&console_mutex);
        cout << "Bloque " << data->index << " descomprimido exitosamente. "
             << "Comprimido: " << data->compressedData.size()
             << " -> Descomprimido: " << decompressedSize << " bytes" << endl;
        pthread_mutex_unlock(&console_mutex);
    } else {
        data->success = false;
        pthread_mutex_lock(&console_mutex);
        cerr << "Error al descomprimir bloque " << data->index
             << " (codigo " << result << ")" << endl;
        pthread_mutex_unlock(&console_mutex);
    }

    pthread_exit(nullptr);
}

// funcion de entrada compression
void compress_file(const string& inputFile, const string& outputFile, int cant_threads) {
    auto start = high_resolution_clock::now();

    // abrir archivo de entrada
    ifstream in(inputFile, ios::binary | ios::ate);
    if (!in) {
        cerr << "Error: No se pudo abrir el archivo de entrada: " << inputFile << endl;
        return;
    }

    streamsize totalSize = in.tellg();
    in.seekg(0, ios::beg);

    cout << "Archivo de entrada: " << inputFile << endl;
    cout << "Tamaño original: " << totalSize << " bytes" << endl;
    cout << "Numero de hilos: " << cant_threads << endl;

    // calcular numero de bloques
    int numBlocks = (totalSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (cant_threads > numBlocks) {
        cant_threads = numBlocks;
        cout << "Ajustando numero de hilos a " << cant_threads
             << " (numero de bloques disponibles)" << endl;
    }

    // crear estructura de datos para cada bloque
    vector<CompressionData> blocks(numBlocks);
    vector<pthread_t> threads(cant_threads);

    // leer y dividir archivo en bloques
    for (int i = 0; i < numBlocks; i++) {
        size_t blockSize = BLOCK_SIZE;
        if (i == numBlocks - 1) {
            blockSize = totalSize - i * BLOCK_SIZE;
        }

        blocks[i].index = i;
        blocks[i].originalData.resize(blockSize);

        if (!in.read(blocks[i].originalData.data(), blockSize)) {
            cerr << "Error al leer el bloque " << i << endl;
            return;
        }
    }
    in.close();

    cout << "Archivo dividido en " << numBlocks << " bloques de ~"
         << BLOCK_SIZE / 1024 << " KB cada uno" << endl;

    // procesar bloques con hilos
    int currentBlock = 0;

    while (currentBlock < numBlocks) {
        int threadsToCreate = min(cant_threads, numBlocks - currentBlock);

        // crear hilos para procesar bloques
        for (int t = 0; t < threadsToCreate; t++) {
            pthread_create(&threads[t], nullptr, compress_block, &blocks[currentBlock + t]);
        }

        // esperar a que terminen todos los hilos
        for (int t = 0; t < threadsToCreate; t++) {
            pthread_join(threads[t], nullptr);
        }

        currentBlock += threadsToCreate;
    }

    ofstream out(outputFile, ios::binary);
    if (!out) {
        cerr << "Error: No se pudo crear el archivo de salida: " << outputFile << endl;
        return;
    }

    // escribir header con información de bloques
    uint32_t numBlocksHeader = numBlocks;
    out.write(reinterpret_cast<char*>(&numBlocksHeader), sizeof(numBlocksHeader));

    // escribir tamaños de cada bloque original y comprimido
    for (int i = 0; i < numBlocks; i++) {
        uint32_t originalSize = blocks[i].originalData.size();
        uint32_t compressedSize = blocks[i].compressedSize;
        out.write(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));
        out.write(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));
    }

    // escribir datos comprimidos en orden
    size_t totalCompressedSize = 0;
    pthread_mutex_lock(&output_mutex);
    for (int i = 0; i < numBlocks; i++) {
        if (blocks[i].success) {
            out.write(reinterpret_cast<char*>(blocks[i].compressedData.data()),
                     blocks[i].compressedSize);
            totalCompressedSize += blocks[i].compressedSize;
        } else {
            cerr << "Error: El bloque " << i << " no se comprimio correctamente" << endl;
            out.close();
            pthread_mutex_unlock(&output_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&output_mutex);

    out.close();

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    cout << "\nCOMPRESION COMPLETADA" << endl;
    cout << "Archivo de salida: " << outputFile << endl;
    cout << "Tamaño original: " << totalSize << " bytes" << endl;
    cout << "Tamaño comprimido: " << totalCompressedSize << " bytes" << endl;
    cout << "Porcentaje comprimido: " << (100.0 * totalCompressedSize / totalSize) << "%" << endl;
    cout << "Tiempo de ejecucion: " << duration.count() << " ms" << endl;
}

// funcion de entrada descomprimir
void decompress_file(const string& inputFile, const string& outputFile, int cant_threads) {
    auto start = high_resolution_clock::now();

    ifstream in(inputFile, ios::binary);
    if (!in) {
        cerr << "Error: No se pudo abrir el archivo comprimido: " << inputFile << endl;
        return;
    }

    // leer header
    uint32_t numBlocks;
    in.read(reinterpret_cast<char*>(&numBlocks), sizeof(numBlocks));

    cout << "Archivo comprimido: " << inputFile << endl;
    cout << "Numero de bloques: " << numBlocks << endl;
    cout << "Numero de hilos: " << cant_threads << endl;

    if (cant_threads > static_cast<int>(numBlocks)) {
        cant_threads = numBlocks;
        cout << "Ajustando numero de hilos a " << cant_threads << endl;
    }

    // leer información de tamaños
    vector<DecompressionData> blocks(numBlocks);
    for (int i = 0; i < static_cast<int>(numBlocks); i++) {
        uint32_t originalSize, compressedSize;
        in.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));
        in.read(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));

        blocks[i].index = i;
        blocks[i].originalSize = originalSize;
        blocks[i].compressedData.resize(compressedSize);
    }

    // leer datos comprimidos
    for (int i = 0; i < static_cast<int>(numBlocks); i++) {
        in.read(reinterpret_cast<char*>(blocks[i].compressedData.data()),
               blocks[i].compressedData.size());
    }
    in.close();

    // procesar bloques con hilos
    vector<pthread_t> threads(cant_threads);
    int currentBlock = 0;

    while (currentBlock < static_cast<int>(numBlocks)) {
        int threadsToCreate = min(cant_threads, static_cast<int>(numBlocks) - currentBlock);

        // crear hilos para descomprimir bloques
        for (int t = 0; t < threadsToCreate; t++) {
            pthread_create(&threads[t], nullptr, decompress_block,
                          &blocks[currentBlock + t]);
        }

        // esperar a que terminen todos los hilos
        for (int t = 0; t < threadsToCreate; t++) {
            pthread_join(threads[t], nullptr);
        }

        currentBlock += threadsToCreate;
    }

    // escribir archivo descomprimido manteniendo el orden
    ofstream out(outputFile, ios::binary);
    if (!out) {
        cerr << "Error: No se pudo crear el archivo de salida: " << outputFile << endl;
        return;
    }

    size_t totalDecompressedSize = 0;
    pthread_mutex_lock(&output_mutex);
    for (int i = 0; i < static_cast<int>(numBlocks); i++) {
        if (blocks[i].success) {
            out.write(blocks[i].decompressedData.data(),
                     blocks[i].decompressedData.size());
            totalDecompressedSize += blocks[i].decompressedData.size();
        } else {
            cerr << "Error: El bloque " << i << " no se descomprimio correctamente" << endl;
            out.close();
            pthread_mutex_unlock(&output_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&output_mutex);

    out.close();

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    cout << "\nDESCOMPRESION COMPLETADA" << endl;
    cout << "Archivo de salida: " << outputFile << endl;
    cout << "Tamaño descomprimido: " << totalDecompressedSize << " bytes" << endl;
    cout << "Tiempo de ejecucion: " << duration.count() << " ms" << endl;
}

// verificar integridad
void verify_integrity(const string& original, const string& decompressed) {
    ifstream orig(original, ios::binary);
    ifstream decomp(decompressed, ios::binary);

    if (!orig || !decomp) {
        cerr << "Error: No se pudieron abrir los archivos para verificacion" << endl;
        return;
    }

    orig.seekg(0, ios::end);
    decomp.seekg(0, ios::end);

    if (orig.tellg() != decomp.tellg()) {
        cout << "VERIFICACION: FALLO - Los archivos tienen diferentes tamaños" << endl;
        return;
    }

    orig.seekg(0, ios::beg);
    decomp.seekg(0, ios::beg);

    const size_t BUFFER_SIZE = 4096;
    vector<char> buffer1(BUFFER_SIZE), buffer2(BUFFER_SIZE);

    while (orig && decomp) {
        orig.read(buffer1.data(), BUFFER_SIZE);
        decomp.read(buffer2.data(), BUFFER_SIZE);

        if (orig.gcount() != decomp.gcount() ||
            memcmp(buffer1.data(), buffer2.data(), orig.gcount()) != 0) {
            cout << "VERIFICACION: FALLO - Los archivos son diferentes" << endl;
            return;
        }
    }

    cout << "VERIFICACION: EXITOSA" << endl;
}

int main() {
    char opcion;

    cout << "BIENVENIDO" << endl;
    cout << "Tamaño de bloque: " << BLOCK_SIZE / 1024 << " KB" << endl;

    while (true) {
        cout << "\n1. Comprimir archivo" << endl;
        cout << "2. Descomprimir archivo" << endl;
        cout << "3. Verificar integridad" << endl;
        cout << "0. Salir" << endl;
        cout << ">>> ";
        cin >> opcion;

        switch (opcion) {
            case '1': {
                int cant_threads;
                string inputFile, outputFile;

                cout << "\nCOMPRESION" << endl;
                cout << "Numero de hilos: ";
                cin >> cant_threads;

                if (cant_threads <= 0) {
                    cerr << "Error: El numero de hilos debe ser mayor a 0" << endl;
                    break;
                }

                cout << "Archivo de entrada: ";
                cin >> inputFile;
                cout << "Archivo de salida: ";
                cin >> outputFile;

                compress_file(inputFile, outputFile, cant_threads);
                break;
            }

            case '2': {
                int cant_threads;
                string inputFile, outputFile;

                cout << "\nDESCOMPRESION" << endl;
                cout << "Numero de hilos: ";
                cin >> cant_threads;

                if (cant_threads <= 0) {
                    cerr << "Error: El numero de hilos debe ser mayor a 0" << endl;
                    break;
                }

                cout << "Archivo comprimido: ";
                cin >> inputFile;
                cout << "Archivo de salida: ";
                cin >> outputFile;

                decompress_file(inputFile, outputFile, cant_threads);
                break;
            }

            case '3': {
                string originalFile, decompressedFile;

                cout << "\nVERIFICACION" << endl;
                cout << "Archivo original: ";
                cin >> originalFile;
                cout << "Archivo descomprimido: ";
                cin >> decompressedFile;

                verify_integrity(originalFile, decompressedFile);
                break;
            }

            case '0':
                cout << "Bye" << endl;
                return 0;

            default:
                cout << "Opcion invalida" << endl;
        }
    }

    return 0;
}