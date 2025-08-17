#include "sensor_bno.h"

static Adafruit_BNO055 bno(55, 0x28);

bool BNO_begin(uint8_t address, int32_t id) {
  // Ricrea l'oggetto con i parametri desiderati
  bno = Adafruit_BNO055(id, address);
  if (!bno.begin()) return false;
  bno.setExtCrystalUse(true);
  return true;
}

void BNO_useExtCrystal(bool use) { bno.setExtCrystalUse(use); }

EulerAngles BNO_readEuler() {
  imu::Vector<3> e = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
  EulerAngles out; out.yaw = e.x(); out.roll = e.y(); out.pitch = e.z();
  return out;
}
