#include "output.h"

void printCsvHeader() {
  Serial.println("vert,lat,tors,inTarget,tempoPos,rep");
}

void printCsvRow(float vert, float lat, float tors,
                 bool inTarget, unsigned long tempoPos,
                 unsigned int rep) {
  Serial.print("DATA"); Serial.print(",");
  Serial.print(vert, 2); Serial.print(",");
  Serial.print(lat, 2);  Serial.print(",");
  if (isnan(tors)) Serial.print("NaN"); else Serial.print(tors, 2);
  Serial.print(",");
  Serial.print(inTarget ? 1 : 0); Serial.print(",");
  Serial.print(tempoPos); Serial.print(",");
  Serial.println(rep);
}
