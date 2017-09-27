#include "apgSampleCmn.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdexcept>


///////////////////////////
// MAKE	  TOKENS
std::vector<std::string> apgSampleCmn::MakeTokens(const std::string &str, const std::string &separator)
{
	std::vector<std::string> returnVector;
	std::string::size_type start = 0;
	std::string::size_type end = 0;

	while( (end = str.find(separator, start)) != std::string::npos)
	{
		returnVector.push_back (str.substr (start, end-start));
		start = end + separator.size();
	}

	returnVector.push_back( str.substr(start) );

	return returnVector;
}

///////////////////////////
//	GET    ITEM    FROM     FIND       STR
std::string apgSampleCmn::GetItemFromFindStr( const std::string & msg,
				const std::string & item )
{

	//search the single device input string for the requested item
    std::vector<std::string> params =  apgSampleCmn::MakeTokens( msg, "," );
	std::vector<std::string>::iterator iter;

	for(iter = params.begin(); iter != params.end(); ++iter)
	{
	   if( std::string::npos != (*iter).find( item ) )
	   {
		 std::string result =  MakeTokens( (*iter), "=" ).at(1);
		 
		 return result;
	   }
	} //for

	std::string noOp;
	return noOp;
}

////////////////////////////
//	GET		USB  ADDRESS
std::string apgSampleCmn::GetUsbAddress( const std::string & msg )
{
    return apgSampleCmn::GetItemFromFindStr( msg, "address=" );
}


////////////////////////////
//	GET		ETHERNET  ADDRESS
std::string apgSampleCmn::GetEthernetAddress( const std::string & msg )
{
    std::string addr = apgSampleCmn::GetItemFromFindStr( msg, "address=" );
    addr.append(":");
    addr.append( apgSampleCmn::GetItemFromFindStr( msg, "port=" ) );
    return addr;
}
////////////////////////////
//	GET		ID
uint16_t apgSampleCmn::GetID( const std::string & msg )
{
    std::string str = apgSampleCmn::GetItemFromFindStr( msg, "id=" );
    uint16_t id = 0;
    std::stringstream ss;
    ss << std::hex << std::showbase << str.c_str();
    ss >> id;
  
    return id;
}

////////////////////////////
//	GET		FRMWR       REV
uint16_t apgSampleCmn::GetFrmwrRev( const std::string & msg )
{
    std::string str = apgSampleCmn::GetItemFromFindStr(  msg, "firmwareRev=" );
   
    uint16_t rev = 0;
    std::stringstream ss;
    ss << std::hex << std::showbase << str.c_str();
    ss >> rev;

    return rev;
}

////////////////////////////
//	        IS      DEVICE      FILTER      WHEEL
bool apgSampleCmn::IsDeviceFilterWheel( const std::string & msg )
{
    std::string str = apgSampleCmn::GetItemFromFindStr(  msg, "deviceType=" );
    
    return ( 0 == str.compare("filterWheel" ) ? true : false );
}

////////////////////////////
//	        IS  	ASCENT
bool apgSampleCmn::IsAscent( const std::string & msg )
{
	std::string model = apgSampleCmn::GetItemFromFindStr(  msg, "model=" );
	std::string ascent("Ascent");
    return( 0 == model .compare( 0, ascent.size(), ascent ) ? true : false );
}

////////////////////////////
//	        IS  	ASPEN
bool apgSampleCmn::IsAspen( const std::string & msg )
{
	std::string model = apgSampleCmn::GetItemFromFindStr(  msg, "model=" );
	std::string aspen("Aspen");
    return( 0 == model .compare( 0, aspen.size(), aspen ) ? true : false );
}

////////////////////////////
//      PRINT       INFO
void apgSampleCmn::printInfo( const std::string & model,
                             const uint16_t maxImgRows,
                             const uint16_t maxImgCols )
{
    std::cout << "Cam Info: " << std::endl;
    std::cout << "model = " << model.c_str() << std::endl;
    std::cout << "max # imaging rows = " << maxImgRows;
    std::cout << "\tmax # imaging cols = " << maxImgCols << std::endl;
}

////////////////////////////
//  SAVE    IMG         2    FILE
void apgSampleCmn::saveImg2File( char * data, const std::string fileName,
					  uint32_t NumBytes2Write )
{
    std::ofstream fout( fileName.c_str(), std::ios::binary );
    fout.write( data, NumBytes2Write );
    fout.close();
}

////////////////////////////
//		CHECK	STATUS
void apgSampleCmn::checkStatus( const Apg::Status status )
{
	switch( status )
	{
		case Apg::Status_ConnectionError:
		{
			std::string errMsg("Status_ConnectionError");
			std::runtime_error except( errMsg );
			throw except;
		}
		break;
	
		case Apg::Status_DataError:
		{
			std::string errMsg("Status_DataError");
			std::runtime_error except( errMsg );
			throw except;
		}
		break;
	
		case Apg::Status_PatternError:
		{
			std::string errMsg("Status_PatternError");
			std::runtime_error except( errMsg );
			throw except;
		}
		break;
	
		case Apg::Status_Idle:
		{
			std::string errMsg("Status_Idle");
			std::runtime_error except( errMsg );
			throw except;
		}
		break;
	
		default:
			//no op on purpose
		break;
	}
	
}

