#include <iostream>
#include <string>
#include <pthread.h>
#include <vector>
#include <fstream>
#include <zlib.h>
#include <mutex>

using namespace std;

struct compression_args {
    int index;
    streamsize size;
    vector<char> buffer;
    vector<Bytef> compressed;
};

// funcion de entrada
void* comprimir_test(void* args) {
    compression_args* data = reinterpret_cast<compression_args*>(args);

    uLongf compressedSize = compressBound(data->size);
    data->compressed.resize(compressedSize);

    int res = compress(data->compressed.data(), &compressedSize,
                       reinterpret_cast<const Bytef*>(data->buffer.data()), data->size);

    if (res != Z_OK) {
        cerr << "Error al comprimir bloque " << data->index << " (codigo " << res << ")" << endl;
        pthread_exit(nullptr);
    }

    // ajustar al tamaño real comprimido
    data->compressed.resize(compressedSize);
    pthread_exit(nullptr);
}

int main() {
    char opcion = ' ';
    while (opcion != '0') {
        cout << "BIENVENIDO\n(1) Comprimir\n(2) Descomprimir archivo\n(0) Salir\n>>> ";
        cin >> opcion;

        if (opcion == '1') {
            int cant_hilos;
            string inputFile, outputFile;
            cout << "- NUMERO DE HILOS: ";
            cin >> cant_hilos;

            cout << "- RUTA AL ARCHIVO DE ENTRADA: ";
            cin >> inputFile;
            cout << "- RUTA AL ARCHIVO DE SALIDA: ";
            cin >> outputFile;

            // abrir archivo de entrada
            ifstream in(inputFile, ios::binary | ios::ate);
            if (!in) {
                cerr << "No se pudo abrir el archivo de entrada." << endl;
                return 1;
            }

            streamsize totalSize = in.tellg();
            in.seekg(0, ios::beg);

            streamsize chunkSize = totalSize / cant_hilos;
            vector<compression_args*> args(cant_hilos);

            // Crear hilos
            pthread_t hilos[cant_hilos];
            for (int i = 0; i < cant_hilos; i++) {
                streamsize thisSize;
                if (i == cant_hilos - 1)
                {
                    thisSize = totalSize - i * chunkSize;
                } else
                {
                    thisSize = chunkSize;
                }

                vector<char> buffer(thisSize);
                if (!in.read(buffer.data(), thisSize)) {
                    cerr << "Error al leer el archivo." << endl;
                    return 1;
                }

                args[i] = new compression_args{
                    i,
                    thisSize,
                    buffer,
                    {}
                };

                pthread_create(&hilos[i], NULL, comprimir_test, args[i]);
            }
            in.close();

            // joins
            for (int i = 0; i < cant_hilos; i++) {
                pthread_join(hilos[i], nullptr);
            }

            // archivo de salida
            ofstream out(outputFile, ios::binary);
            if (!out) {
                cerr << "No se pudo abrir el archivo de salida." << endl;
                return 1;
            }

            size_t totalCompressed = 0;
            for (int i = 0; i < cant_hilos; i++) {
                out.write(reinterpret_cast<char*>(args[i]->compressed.data()),
                args[i]->compressed.size());
                totalCompressed += args[i]->compressed.size();
                delete args[i];
            }
            out.close();

            cout << "Archivo comprimido exitosamente: " << outputFile << endl;
            cout << "Tamano original: " << totalSize << " bytes" << endl;
            cout << "Tamano comprimido: " << totalCompressed << " bytes" << endl;
        }
    }
    return 0;
}