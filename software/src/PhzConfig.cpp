/* Phazerville Config File
 * stored on LittleFS in flash storage
 * supercedes previous EEPROM mechanism
 */
#ifdef __IMXRT1062__
#include <LittleFS.h>
#include "PhzConfig.h"

// NOTE: This option is only available on the Teensy 4.0, Teensy 4.1 and Teensy Micromod boards.
// With the additonal option for security on the T4 the maximum flash available for a
// program disk with LittleFS is 960 blocks of 1024 bytes
#define PROG_FLASH_SIZE 1024 * 512 // Specify size to use of onboard Teensy Program Flash chip
                                   // This creates a LittleFS drive in Teensy PCB FLash.

namespace PhzConfig {

const char * const CONFIG_FILENAME = "PEWPEW.CFG";
const char * const EEPROM_FILENAME = "EEPROM.DAT";

LittleFS_Program myfs;
File dataFile;  // Specifes that dataFile is of File type

int record_count = 0;
uint32_t diskSize;

void setup()
{
  diskSize = PROG_FLASH_SIZE;
  // checks that the LittFS program has started with the disk size specified
  if (!myfs.begin(diskSize)) {
    Serial.println("LittleFS unavailable!! Settings WILL NOT BE SAVED!");
    //while (1) { }
  }
  Serial.println("LittleFS initialized.");

  // opens a file or creates a file if not present,  FILE_WRITE will append data to
  // to the file created.
  if (myfs.mediaPresent()) {
    listFiles();

    load_config();
  }
}

void save_config()
{
    dataFile = myfs.open(CONFIG_FILENAME, FILE_WRITE);
    if (dataFile) {
      // make a string for assembling the data to log:
      String dataString = "";

      int sensor = analogRead(0);
      dataString += String(sensor);
      dataString += ",";

      dataFile.println(dataString);
      // print to the serial port too:
      Serial.println(dataString);
      record_count += 1;
    } else {
      // if the file isn't open, pop up an error:
      Serial.printf("error opening %s\n", CONFIG_FILENAME);
    }
}

void close()
{
  Serial.println("\nConfig file closed.");
  // Closes the data file.
  dataFile.close();
  Serial.printf("Records written = %d\n", record_count);
}


void load_config()
{
  Serial.println("\nDumping Config!!!");
  dataFile = myfs.open(CONFIG_FILENAME);

  if (dataFile) {
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
  } else {
    Serial.printf("error opening %s\n", CONFIG_FILENAME);
  }
}

void listFiles()
{
  Serial.print("\n Space Used = ");
  Serial.println(myfs.usedSize());
  Serial.print("Filesystem Size = ");
  Serial.println(myfs.totalSize());

  printDirectory(myfs);
}

void eraseFiles()
{
  myfs.quickFormat();  // performs a quick format of the created di
  Serial.println("\nFiles erased !");
}

void printDirectory(FS &fs) {
  Serial.println("Directory\n---------");
  printDirectory(fs.open("/"), 0);
  Serial.println();
}

void printDirectory(File dir, int numSpaces) {
   while(true) {
     File entry = dir.openNextFile();
     if (! entry) {
       //Serial.println("** no more files **");
       break;
     }
     printSpaces(numSpaces);
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numSpaces+2);
     } else {
       // files have sizes, directories do not
       printSpaces(36 - numSpaces - strlen(entry.name()));
       Serial.print("  ");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}

void printSpaces(int num) {
  for (int i=0; i < num; i++) {
    Serial.print(" ");
  }
}

} // namespace PhzConfig
#endif
