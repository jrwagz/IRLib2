/* IRLib_P13_DYNASTY20LASERTAG.h
 * Part of IRLib Library for Arduino receiving, decoding, and sending
 * infrared signals. See COPYRIGHT.txt and LICENSE.txt for more information.
 */
/*
 * This is the implementation of the DYNASTY20LASERTAG IR Protocol encode/decode functions
    
    Overall data structure of an DYNASTY20LASERTAG protocol
    1600uSec header (transmitter ON)
    40 alternating off/on time slots 
    slot 1 = OFF, slot 2 = ON, slot 3 = OFF, slot 4 = ON, etc
    the duration of each time slot decides whether it's a 1 or a 0
    400uSec or 800uSec each, 400uSec = 0, 800uSec = 1
    each slot is serialized in sequential order after the header, meaning that slot 1 comes first, then slot 2, etc.

    This means that there are 40 bits of information in each sequence, hence why the send function
    takes both a uint32_t (data) and a uint8_t (data2) they will be serialized as follows:

    data2[7] -> data2[6] -> data2[5] -> data2[4] -> data2[3] -> data2[2] -> data2[1] -> data2[0] ->
    data[31] -> data[30] -> data[29] -> data[28] -> data[27] -> data[26] -> data[25] -> data[24] ->
    data[23] -> data[22] -> data[21] -> data[20] -> data[19] -> data[18] -> data[17] -> data[16] ->
    data[15] -> data[14] -> data[13] -> data[12] -> data[11] -> data[10] -> data[9]  -> data[9]  ->
    data[7]  -> data[6]  -> data[5]  -> data[4]  -> data[3]  -> data[2]  -> data[1]  -> data[0]

    This matches the protocol decoding performed by AnalysIR for the protocol with the same name, DYNASTY20LASERTAG
 */

#ifndef IRLIB_PROTOCOL_13_H
#define IRLIB_PROTOCOL_13_H
#define IR_SEND_PROTOCOL_13		case 13: IRsendDYNASTY20LASERTAG::send(data,data2); break;
#define IR_DECODE_PROTOCOL_13	if(IRdecodeDYNASTY20LASERTAG::decode()) return true;
#ifdef IRLIB_HAVE_COMBO
	#define PV_IR_DECODE_PROTOCOL_13 ,public virtual IRdecodeDYNASTY20LASERTAG
	#define PV_IR_SEND_PROTOCOL_13   ,public virtual IRsendDYNASTY20LASERTAG
#else
	#define PV_IR_DECODE_PROTOCOL_13  public virtual IRdecodeDYNASTY20LASERTAG
	#define PV_IR_SEND_PROTOCOL_13    public virtual IRsendDYNASTY20LASERTAG
#endif

#define DYNASTY_DATA2_TOPBIT 0x80
#define DYNASTY_TH 1600
#define DYNASTY_T0 400
#define DYNASTY_T1 800
#define DYNASTY_KHZ 38


#ifdef IRLIBSENDBASE_H
class IRsendDYNASTY20LASERTAG: public virtual IRsendBase {
  public:
    void IRsendDYNASTY20LASERTAG::sendWithTeamAndWeapon(uint32_t teamValue, uint32_t weaponValue) {
      // slots 1-8   : random value chosen at bootup time (8 bits) - doesn't change with weapon/team choice, always the same after each boot of the gun
      //             : for simplicity stay with 0xAE for this field
      // slots 9-16  : 0b10101010 (0xAA) fixed value (8 bits) [slot 9 = 1, slot 10 = 0, etc]
      // slots 17-24 : Team Code (8 bits)
      // slots 25-32 : Weapon Code (8 bits)
      // slots 33-36 : 0b0000 (0x0) fixed value (4 bits)
      // slots 37-40 : random value (?????) (4 bits) - some sort of CRC/hash based on all the other slots, 
      //             : it's not clear what the entire function is for any random value in slots 1-8
      //             : but when slots 1-8 equals 0xAE, we know the formula is as follows:
      //             : value = (0xD + team_code + weapon_code) % (modulus) 0xF
      uint8_t randByte = 0xAE;
      uint32_t fixedByte = 0xAA;
      uint32_t checksumOffset = 0x0D;
      uint32_t checksum = 0x0;

      uint32_t data;
      uint8_t  data2;

      // Checksum is calculated as follows
      checksum = checksumOffset + teamValue + weaponValue;
      checksum = checksum & 0x000F;

      // The full 40 bit value is encoded as follows:
      // {randByte, fixedByte, teamValue, weaponValue, checksum}
      data = (fixedByte << 24) | (teamValue << 16) | (weaponValue << 8) | checksum;
      data2 = randByte;

      send(data, data2);
    }
    void IRsendDYNASTY20LASERTAG::send(uint32_t data, uint8_t data2) {
      uint16_t bit_time;

      // 1600uSec header (transmitter ON)
      mark(DYNASTY_TH);

      //   40 alternating off/on time slots 
      // slot 1 = OFF, slot 2 = ON, slot 3 = OFF, slot 4 = ON, etc
      // the duration of each time slot decides whether it's a 1 or a 0
      // 400uSec or 800uSec each, 400uSec = 0, 800uSec = 1
      
      // First send data2
      for (uint8_t i = 0; i < 8; i++ ) {
        // Decide how long it needs to be
        if (data2 & DYNASTY_DATA2_TOPBIT) {
          bit_time = DYNASTY_T1;
        } else {
          bit_time = DYNASTY_T0;
        } 
        // Now decide if it's a mark or a space
        if (i & 1) {
          // Odds are marks
          mark(bit_time);
        } else {
          // evens are spaces
          space(bit_time);
        }
        data2 <<= 1;
      }
      // Next send data
      for (uint8_t i = 0; i < 32; i++ ) {
        // Decide how long it needs to be
        if (data & TOPBIT) {
          bit_time = DYNASTY_T1;
        } else {
          bit_time = DYNASTY_T0;
        } 
        // Now decide if it's a mark or a space
        if (i & 1) {
          // Odds are marks
          mark(bit_time);
        } else {
          // evens are spaces
          space(bit_time);
        }
        data <<= 1;
      }

      space(1000); // Just to be sure
    };
};
#endif  //IRLIBSENDBASE_H

#ifdef IRLIBDECODEBASE_H
class IRdecodeDYNASTY20LASERTAG: public virtual IRdecodeBase {
  public:
    bool IRdecodeDYNASTY20LASERTAG::decode(void) {
      IRLIB_ATTEMPT_MESSAGE(F("DYNASTY20LASERTAG"));
      resetDecoder();//This used to be in the receiver getResults.
      /*********
       *  Insert your code here. Return false if it fails. 
       *  Don't forget to include the following lines or 
       *  equivalent somewhere in the code.
       *  
       *  bits = 40;	//Substitute proper value here
       *  value = data;	//return data in "value"
       *  protocolNum = DYNASTY20LASERTAG;	//set the protocol number here.
       */

      // Since this is a 40 bit value, we will pass it back as follows:
      // data[39:32] -> address[7:0]
      // data[31:0]  -> value[31:0]
      // TODO: IMPLEMENT ME!!!!!
      bits = 40;
      protocolNum = DYNASTY20LASERTAG;
      return true;
    }
};

#endif //IRLIBDECODEBASE_H

#define IRLIB_HAVE_COMBO

#endif //IRLIB_PROTOCOL_13_H
