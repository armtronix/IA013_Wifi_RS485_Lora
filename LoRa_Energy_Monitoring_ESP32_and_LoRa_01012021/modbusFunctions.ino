/* The preTransmission() function initializes the MAX485_DE pin to start writing by setting the binary 1. */ 
void preTransmission()
 {
   digitalWrite(MAX485_DE, 1);
 }
 
/*###############################################################################################################################################################################*/
/* the post Transmission() function passes values of 0 to the MAX485_DE pin to stop the transmission. */
void postTransmission()
 {
   digitalWrite(MAX485_DE, 0);
 }
 
/*###############################################################################################################################################################################*/
String str_Addzero(uint16_t a)
 {
   String strL_Tempzero = String(a, HEX);
   if(strL_Tempzero.length() == 3)
    {
      strL_Tempzero = "0" + strL_Tempzero;
    }
   else if(strL_Tempzero.length() == 2)
    {
      strL_Tempzero = "00" + strL_Tempzero;
    }
   else if(strL_Tempzero.length() == 1)
    {
      strL_Tempzero = "000" + strL_Tempzero;
    }
   return strL_Tempzero;
 }
 
/*###############################################################################################################################################################################*/
float f_hex_to_floatingP(uint16_t Data2, uint16_t Data1)
 {
   float fL_Data_Float;
   String strL_Tempdata;
   uint8_t ucL_Count = 0;

   strL_Tempdata = str_Addzero(Data1) + str_Addzero(Data2);

   _data.i = strtoul((char*)strL_Tempdata.c_str(), NULL, 16);
   fL_Data_Float = _data.f;

   for(ucL_Count = 0; ucL_Count <= 1; ucL_Count ++)
    {
      if(fL_Data_Float == 0.00 && ucL_Count <= 1)
       {
         _data.i = strtoul((char*)strL_Tempdata.c_str(), NULL, 16);
         fL_Data_Float = _data.f;
       }
      else
       {
         return (fL_Data_Float);
       }
    }
   return (fL_Data_Float);
 }
 
/*###############################################################################################################################################################################*/
/* The function get_data() is called to read the values from each register adresses of the energy meter and store it in the array named as frgG_DataArray[j] */
void get_data(uint8_t ucL_Saddress , uint8_t ucL_Arange )
 {
   for(uint8_t i = ( ucL_Saddress-100); i <= (ucL_Arange-2+( ucL_Saddress-100)); i = i + 2)
    { 
      uint8_t j = i / 2;
      frgG_DataArray[j] = f_hex_to_floatingP(node.getResponseBuffer(i), node.getResponseBuffer(i + 1));
    }
 }
 
/*###############################################################################################################################################################################*/
void handle_rs485(uint8_t ucL_Start_Address, uint8_t ucL_Range)
 { 
   for(int iL_Rs485_Read_Error =0 ; iL_Rs485_Read_Error <=RS485_ERROR_REREAD ; iL_Rs485_Read_Error++)          // Re-read RS485 if we didn't get the reading for 2 times.
    {
      delay(10);
      ucG_Result = node.readHoldingRegisters(START_ADDRESS , RANGE);                                           // Read all the register values of the energy meter.

      get_data(ucL_Start_Address,ucL_Range);
      //digitalWrite(ERRLED, 0);
/*If reading RS485 is successful then display errorcode as NONE and break the for loop.*/
      if(ucG_Result == node.ku8MBSuccess)                                                            
       {
         Serial.println(" rs485 sucessful");
         strG_Errorcode_Check = "NONE";
         break;
       }
/*If reading RS485 is unsuccessful then again read it for one more time then also its unsuccessful, display errorcode as E2.*/
      else
       {
         //digitalWrite(ERRLED, 1);
         if(iL_Rs485_Read_Error == RS485_REREAD)
          {
            Serial.println(" rs485 unsucessful");
            strG_Errorcode_Check = "Error:";
          }    
         Serial1.end();
         F_Rs485SerialSetup();
       }
    }
 }
