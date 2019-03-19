/* EDL_DLL.DLL
Library to provide Python ctypes access to edl.dll library. The edl static library,
edl.lib is linked into this project.
EYafuso, 2019
*/

#include <iostream>
#include "windows.h"
#include "edl.h"

#define MINIMUM_DATA_PACKETS_TO_READ 10

static EDL edl;

extern "C" __declspec(dllexport) int initEDL()
{
    // Declare an #EdlErrorCode_t to be returned from #EDL methods.
    EdlErrorCode_t res;

    // Initialize a vector of strings to collect the detected devices.
    std::vector <std::string> devices;
    std::ios::sync_with_stdio(true);

    // Detect plugged in devices
    res = edl.detectDevices(devices);
    if (res != EdlSuccess) {return -1;}

    //Connect
    res = edl.connectDevice(devices.at(0));
    if (res != EdlSuccess) {return -1;}

    edl.purgeData();

    return 0;
}

extern "C" __declspec(dllexport)void configureWorkingModality()
{
    EdlCommandStruct_t commandStruct;

	// Set the sampling rate to 5kHz. Stack the command (do not apply)
    commandStruct.radioId = EDL_RADIO_SAMPLING_RATE_5_KHZ;
    edl.setCommand(EdlCommandSamplingRate, commandStruct, false);

	// Set the current range to 200pA. Stack the command (do not apply).
    commandStruct.radioId = EDL_RADIO_RANGE_200_PA;
    edl.setCommand(EdlCommandRange, commandStruct, false);

	// Disable current filters (final bandwidth equal to half sampling rate). Apply all of the stacked commands.
    commandStruct.radioId = EDL_RADIO_FINAL_BANDWIDTH_SR_2;
    edl.setCommand(EdlCommandFinalBandwidth, commandStruct, true);
}

extern "C" __declspec(dllexport)void compensateDigitalOffset()
{
    EdlCommandStruct_t commandStruct;

	// Select the constant protocol: protocol 0.
    commandStruct.value = 0.0;
    edl.setCommand(EdlCommandMainTrial, commandStruct, false);

	// Set the vHold to 0mV.
    commandStruct.value = 0.0;
    edl.setCommand(EdlCommandVhold, commandStruct, false);
    edl.setCommand(EdlCommandApplyProtocol, commandStruct, true);

    // Start the digital compensation.
    commandStruct.buttonPressed = EDL_BUTTON_PRESSED;
    edl.setCommand(EdlCommandCompAll, commandStruct, true);

    Sleep(5000);

    // Stop the digital compensation.
    commandStruct.buttonPressed = EDL_BUTTON_RELEASED;
    edl.setCommand(EdlCommandCompAll, commandStruct, true);
}

extern "C" __declspec(dllexport)void setPotential()
{
    EdlCommandStruct_t commandStruct;

    commandStruct.value = 1.0;
    edl.setCommand(EdlCommandMainTrial, commandStruct, false);

    /*! Set the vHold to 0mV. */
    commandStruct.value = 0.0;
    edl.setCommand(EdlCommandVhold, commandStruct, false);

    /*! Set the triangular wave amplitude to 50mV: 100mV positive to negative delta voltage. */
    commandStruct.value = 50.0;
    edl.setCommand(EdlCommandVamp, commandStruct, false);

    /*! Set the triangular period to 100ms. */
    commandStruct.value = 100.0;
    edl.setCommand(EdlCommandTPeriod, commandStruct, false);

    /*! Apply the protocol. */
    edl.setCommand(EdlCommandApplyProtocol, commandStruct, true);
}

/*! \fn readAndSaveSomeData
 * \brief Reads data from the EDL device and writes them on an open file.
 */
extern "C" __declspec(dllexport)EdlErrorCode_t readData(FILE * f)
{
    EdlErrorCode_t res;
    EdlDeviceStatus_t status;

	// Variable for the number of read data packets.
    unsigned int readPacketsNum;

	// Vector to collect the read data packets.
    std::vector <float> data;

    Sleep(500);

	res = edl.purgeData();
    if (res != EdlSuccess) {return res;}

	// Data collection
	unsigned int c;
    for (c = 0; c < 1e3; c++) {
		// Current status shows number of available data packets EdlDeviceStatus_t::availableDataPackets.
        res = edl.getDeviceStatus(status);
        if (res != EdlSuccess) {return res;}
		if (status.bufferOverflowFlag) {}
		if (status.lostDataFlag) {}
        if (status.availableDataPackets >= MINIMUM_DATA_PACKETS_TO_READ) {
		    // If at least MINIMUM_DATA_PACKETS_TO_READ data packet are available read them.
			res = edl.readData(status.availableDataPackets, readPacketsNum, data);
            if (res == EdlDeviceNotConnectedError)
            {
                fclose(f);
				return res;
			}
			else
			{
				if (res == EdlNotEnoughAvailableDataError)
                {
				}

	            /* Output vector: readPacketsNum data packets of EDL_CHANNEL_NUM floating point values
				 * The first item in each data packet is the value voltage channel [mV];
				 * following are values of current channels in pA or nA, depending on value assigned to EdlCommandSamplingRate. */
                for (unsigned int readPacketsIdx = 0; readPacketsIdx < readPacketsNum; readPacketsIdx++)
                {
                    for (unsigned int channelIdx = 0; channelIdx < EDL_CHANNEL_NUM; channelIdx++)
                    {
                        fwrite((unsigned char *)&data.at(readPacketsIdx*EDL_CHANNEL_NUM+channelIdx), sizeof(float), 1, f);
                    }
                }
			}
        } else {Sleep(1);}
    }
    return res;
}

extern "C" __declspec(dllexport) EdlErrorCode_t closeEDL()
{
    EdlErrorCode_t res;
    unsigned int c = 0;
    while (c++ < 1e3)
    {
		res = edl.disconnectDevice();
		if (res == EdlSuccess) {break;}
        Sleep(1);
    }

    return res;
}
