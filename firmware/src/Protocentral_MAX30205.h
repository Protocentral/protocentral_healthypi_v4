//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino library for the MAX30205 body temperature sensor breakout board
//
//    Author: Ashwin Whitchurch
//    Copyright (c) 2018 ProtoCentral
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//   For information on how to use, visit https://github.com/protocentral/ProtoCentral_MAX30205
/////////////////////////////////////////////////////////////////////////////////////////
#ifndef Protocentral_MAX30205_h
#define Protocentral_MAX30205_h

#define MAX30205_ADDRESS        0x48  // 8bit address converted to 7bit

// Registers
#define MAX30205_TEMPERATURE    0x00  //  get temperature ,Read only
#define MAX30205_CONFIGURATION  0x01  //
#define MAX30205_THYST          0x02  //
#define MAX30205_TOS            0x03  //

#define MAX30205_READ_INTERVAL  500

typedef unsigned char uint8_t;

typedef enum{   	//For configuration registers
  SHUTDOWN,    		// shutdwon mode to reduce power consumption <3.5uA
  COMPARATOR,    	// Bit 0 = operate OS in comparator mode, 1= INTERRUPT MODE
  OS_POLARITY,    	// Polarity bit ;Bit 0 = Active low output, Bit 1 = Active high
  FAULT_QUEUE_0,    // Fault indication bits
  FAULT_QUEUE_1,    // Fault indication bits
  DATA_FORMAT,      //Data Format
  TIME_OUT,          //Time out
  ONE_SHOT           //1= One shot, 0 = Continuos
}configuration;



class MAX30205
{
  public:
   float temperature = 0;      // Last temperature
   void  shutdown(void);   // Instructs device to power-save
   void  printRegisters(void); // Dumps contents of registers for debug
   void  begin(void);
   float getTemperature(void);

  private:
    void    I2CwriteByte(uint8_t address, uint8_t subAddress, uint8_t data);
    void    I2CreadBytes(uint8_t address, uint8_t subAddress, uint8_t * dest, uint8_t count);
    uint8_t I2CreadByte(uint8_t address, uint8_t subAddress);   
};

#endif
