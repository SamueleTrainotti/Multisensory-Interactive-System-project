#ifndef OUTPUT_H
#define OUTPUT_H
#include <Arduino.h>

void printCsvHeader();
void printCsvRow(float vert, float lat, float tors,
                 bool inTarget, unsigned long tempoPos,
                 unsigned int rep);

#endif // OUTPUT_H
