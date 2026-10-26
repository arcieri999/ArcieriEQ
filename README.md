# Arcieri EQ (VST3)

Ecualizador paramétrico en JUCE: interfaz negra y morada, analizador de espectro que se enciende en morado según el nivel de cada frecuencia, y el logo en la cabecera.

## Funciones
- 4 bandas iniciales, hasta 8 (doble clic en la gráfica o botón "+ Banda"; clic derecho sobre un nodo para eliminar).
- Rango de ganancia ajustable (perilla RANGO, de 3 a 48 dB; por defecto ±18 dB).
- Q ajustable por banda (perilla Q o rueda del mouse sobre el nodo).
- Tipo de filtro por banda: Peak, Low Shelf, High Shelf, Low Pass, High Pass, Notch, Band Pass.
- Eje de frecuencias con las altas a la izquierda y las bajas a la derecha.

## Compilar (Windows)
1. Instala Visual Studio 2022 (carga de trabajo "Desarrollo para el escritorio con C++"), CMake y Git.
2. En esta carpeta ejecuta:

       git clone --depth 1 https://github.com/juce-framework/JUCE.git
       cmake -B build -G "Visual Studio 17 2022"
       cmake --build build --config Release

3. El VST3 queda en `build/ArcieriEQ_artefacts/Release/VST3/Arcieri EQ.vst3`.
   Copia esa carpeta a `C:\Program Files\Common Files\VST3\` y reescanea plugins en tu DAW.
   También se genera una versión Standalone para probarlo sin DAW.

## Compilar (macOS / Linux)
Mismos comandos, usando `cmake -B build` y `cmake --build build --config Release`.
