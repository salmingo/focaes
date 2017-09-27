/*! 
* 
* Copyright(c) 2011 Apogee Imaging Systems, Inc. 
* \namespace apogeeCommon
* \brief namespace common functions used by the libapogee sampels
* 
*/ 
#ifndef APGSAMPLECMN_INCLUDE_H__ 
#define APGSAMPLECMN_INCLUDE_H__

#include <string>
#include <vector>
#include <stdint.h>
#include <apogee/CameraInfo.h>

namespace apgSampleCmn
{
    
    std::vector<std::string>MakeTokens(const std::string &str,
									   const std::string &separator);
    
    std::string GetItemFromFindStr( const std::string & msg,
				const std::string & item );
    
    std::string GetUsbAddress( const std::string & msg );
	
	std::string GetEthernetAddress( const std::string & msg );
    
    uint16_t GetID( const std::string & msg );
    
    uint16_t GetFrmwrRev( const std::string & msg );
	
	bool IsDeviceFilterWheel( const std::string & msg );
	
	bool IsAscent( const std::string & msg );
	
	bool IsAspen( const std::string & msg );
	
	void printInfo( const std::string & model, uint16_t maxImgRows,
				   uint16_t maxImgCols );
    
	void saveImg2File( char * data, const std::string fileName,
					  uint32_t  NumBytes2Write );
	
	void checkStatus( Apg::Status status );
}

#endif
