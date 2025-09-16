# Laboratorio 7: Compresión de Archivos en Paralelo

---

## Mecanismos de sincronización

### Pthread Mutex (pthread_mutex_t)

#### **output_mutex**

Se implementó un objeto mutex para proteger la escritura de bloques comprimidos o descomprimidos al archivo de salida.

**Justificación**: para mantener el orden correcto de los bloques en el archivo final. Sin esta sincronización, los hilos podrían escribir simultáneamente, causando corrupción de datos.

#### **console_mutex**

Al igual que en laboratorios anteriores, se utilizó un objeto mutex para sincronizar prints en consola.

**Justificación**: evita que los mensajes de progreso de diferentes hilos se mezclen, haciendo que la salida sea legible.

#### Implementación en el Código

```cpp
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;

// console
pthread_mutex_lock(&console_mutex);
cout << "mensaje..." << thread_id << endl;
pthread_mutex_unlock(&console_mutex);

// escritura del archivo de salida
pthread_mutex_lock(&output_mutex);
for (int i = 0; i < numBlocks; i++) {
    out.write(reinterpret_cast<char*>(blocks[i].data), blocks[i].size);
}
pthread_mutex_unlock(&output_mutex);
```

## 2. Justificación de Necesidad y Descripción de Implementación

La sincronización empleada soluciona diversos problemas relacionados al orden de la salida, tanto en consola como en la escritura del archivo. Sin sincronización se tendrían problemas como condiciones de carrera por el acceso de multiples hilos al archivo y corrupción de datos por escrituras sin orden. Dentro de los recursos compartidos con los que se trabaja están:

1. **Archivo de salida**: todos los hilos necesitan escribir sus resultados
2. **Consola**: Múltiples hilos reportan progreso simultáneamente
3. **Orden de bloques**: para la integridad del archivo final

### Diseño de la Solución

En mi solución, se dividió el archivo de entrada en bloques de 1MB, y se dividió la compresión en multiples hilos, repartiendo el contenido separado por bloques. La aplicación de la sincroniación se dio solo en la escritura del archivo de salida, dejando que  los hilos procesan bloques independientemente sin sincronización.

## 3. Comparación de tiempos de Ejecución

**Archivo de prueba:** paralelismo_teoria.txt (100MB)
### Resultados Experimentales

| Hilos | Tiempo Compresión (ms) | Tiempo Descompresión (ms) |
| ----- | ---------------------- | ------------------------- |
| 1     | 414                    | 213                       |
| 2     | 243                    | 205                       |
| 4     | 180                    | 168                       |
| 8     | 172                    | 150                       |
| 16    | 159                    | 151                       |
| 32    | 155                    | 137                       |

De estos resultados se puede observar que el rendimiento optimo se alcanza con 8 hilos.
## 4. Catálogo de Librerías y Funciones Utilizadas

### Librerías estándar

#### **pthread.h**
Implementacion de hilos POSIX

```cpp
pthread_t threads[MAX_THREADS];
pthread_create(&thread, NULL, func, arg);
pthread_join(thread, NULL);               // Esperar terminación
pthread_mutex_t mutex;                    // Tipo mutex
pthread_mutex_lock(&mutex);               // Bloquear mutex
pthread_mutex_unlock(&mutex);             // Liberar mutex
```

#### **zlib.h**
Compresión

```cpp
uLongf compressBound(uLong sourceLen);    // Calcular tamaño máximo comprimido
int compress(dest, destLen, source, sourceLen); // Comprimir datos
int uncompress(dest, destLen, source, sourceLen); // Descomprimir datos
```

#### **chrono**
Medición de tiempo.

```cpp
auto start = high_resolution_clock::now();
auto duration = duration_cast<milliseconds>(end - start);
```

### Funciones principales

#### **compress_file()**

Función que divide el contenido en bloques de 1MB, y lanza los hilos para procesamiento paralelo.

#### **decompress_file()**

Función que lee un header personalizado con la cantidad de bloques, descomprime bloques en paralelo y reconstruye el archivo original.

#### **verify_integrity()**

Compara byte a byte archivo original vs descomprimido utiliza buffers para eficiencia en archivos grandes, reporta integridad de la operación completa.