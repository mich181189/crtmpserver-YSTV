/* 
 *  Copyright (c) 2010,
 *  Gavriloaie Eugen-Andrei (shiretu@gmail.com)
 *
 *  This file is part of crtmpserver.
 *  crtmpserver is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  crtmpserver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with crtmpserver.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAS_PROTOCOL_RTMP
#include "rtmpappprotocolhandler.h"
#include "protocols/rtmp/basertmpprotocol.h"
#include "protocols/rtmp/messagefactories/messagefactories.h"
#include "application/baseclientapplication.h"
#include "streaming/baseinnetstream.h"
#include "streaming/streamstypes.h"
#include <pqxx/pqxx>
#include <string>

using namespace pqxx;
using namespace app_flvplayback;
using namespace std;

//returns true if the database is useable.
bool checkDB(connection* &con,string dbconstr) {
    INFO("Checking DB connection. If it segfaults here, check you DB settings.");
    if(!con || !con->is_open()) {
        try{
            delete con;
            con = new connection(dbconstr);
            INFO("DB Connected!");
        } catch(const std::exception &e) {
            FATAL("%s",e.what());
            
            return false;
        }
    }
    return true;
}

string getConfig(Variant &configuration,string key) {
	Variant temp = configuration[key];
	if(temp != V_STRING) {
		FATAL("No %s option exists in the config file.",key.c_str());
		return "";
	}
	else {
		return (string)temp;
	}
}



RTMPAppProtocolHandler::RTMPAppProtocolHandler(Variant &configuration)
: BaseRTMPAppProtocolHandler(configuration) {
	dbHost = getConfig(configuration,"dbHost");
	dbUser = getConfig(configuration,"dbUser");
	dbPass = getConfig(configuration,"dbPass");
	db = getConfig(configuration,"db");
	INFO("Using database %s on %s",db.c_str(),dbHost.c_str());
        dbconstring="host="+dbHost+" user=" + dbUser + " password=" + dbPass + " dbname="+db;
        try {
            dbconn = new connection(dbconstring);
            INFO("DB Connected!");
        } catch(const std::exception &e) {
            FATAL("%s",e.what());
        }
}

RTMPAppProtocolHandler::~RTMPAppProtocolHandler() {
	delete dbconn;
}

//I disabled auth in the config instead of in the code,hence this is commented out.
/*//Because we use our own IP-based auth, this isn't needed
bool RTMPAppProtocolHandler::AuthenticateInboundAdobe(BaseRTMPProtocol *pFrom,
		Variant & request, Variant &authState) {
			return true;
}*/


//just something to make our lives a little easier, and so we don't have to stare at that freakin' typecast!
string getIP(BaseRTMPProtocol *pFrom) {
	TCPCarrier *tcp = (TCPCarrier*)pFrom->GetIOHandler();
	return inet_ntoa(tcp->GetFarEndpointAddress().sin_addr);
}

bool RTMPAppProtocolHandler::ProcessInvokeGeneric(BaseRTMPProtocol *pFrom,
		Variant &request) {

	std::string functionName = M_INVOKE_FUNCTION(request);
	if (functionName == "getAvailableFlvs") {
		return ProcessGetAvailableFlvs(pFrom, request);
	} else if (functionName == "insertMetadata") {
		return ProcessInsertMetadata(pFrom, request);
	} else if (functionName == "FCUnpublish") {
		return ProcessInvokeFCUnpublish(pFrom,request); //seems the base class doesn't catch this one, so I'm catching it now.
	} else {
		return BaseRTMPAppProtocolHandler::ProcessInvokeGeneric(pFrom, request);
	}
}

bool RTMPAppProtocolHandler::ProcessInvokeFCUnpublish(BaseRTMPProtocol *pFrom,Variant &request) {
  string streamNameFull = M_INVOKE_PARAM(request, 1);
	size_t len = streamNameFull.find('?');
	string streamName;
	if(len != string::npos)
		streamName = streamNameFull.substr(0,len);
	else
		streamName = streamNameFull;
        if(checkDB(dbconn,dbconstring)) {
            try {
                work dbwork(*dbconn);
                dbwork.exec("UPDATE red5_streams set is_current=false WHERE shortname='" + streamName + "'");
                dbwork.commit();
            } catch(const std::exception &e) {
                FATAL("%s",e.what());
                return false;//something went wrong, so be mean as a safe option.
            }
            
        }
    INFO("Stream %s unpublished.",streamName.c_str());
    return true;
}

bool RTMPAppProtocolHandler::ProcessInvokePlay(BaseRTMPProtocol *pFrom,Variant &request) {
    string streamNameFull = M_INVOKE_PARAM(request, 1);
    size_t len = streamNameFull.find('?');
    string streamName;
    if(len != string::npos)
        streamName = streamNameFull.substr(0,len);
    else
	    streamName = streamNameFull;
    string ip = getIP(pFrom);
    INFO("Play from %s on stream %s",ip.c_str(),streamName.c_str());
    Variant data;
    
    uint32_t start = time(NULL);
    data["time"] = start;
    if(checkDB(dbconn,dbconstring)) {
        try {
            //first make sure we're not trying to access a stream that's not broadcast:
            work dbwork(*dbconn);
            result res = dbwork.exec("SELECT * FROM red5_streams WHERE shortname='" + streamName + "' AND enabled = true AND is_current = true");
            dbwork.commit();
            if(res.size() != 0) {
                //the stream is valid
                work moredbwork(*dbconn);
                stringstream query;
                query << "INSERT INTO stream_hits (start_time,ip_address,client_info,stream_server_id,streamname) VALUES(TIMESTAMP 'epoch' + " << start << " * INTERVAL '1 second','" << ip << "','Unknown',0,'" + streamName + "')";
                moredbwork.exec(query.str());
                result res = moredbwork.exec("SELECT last_value FROM stream_hits_id_seq");
                moredbwork.commit();
                data["dbid"] = res[0][0].c_str();
                INFO("DB row ID: %s",((string)data["dbid"]).c_str());
            }
        } catch(const std::exception &e) {
            FATAL("%s",e.what());
            
        }
    }
    connections[pFrom->GetId()] = data;
    return BaseRTMPAppProtocolHandler::ProcessInvokePlay(pFrom,request);
}

void RTMPAppProtocolHandler::client_close(uint32_t id) {
    Variant client = connections[id];
    if(client == V_NULL)
        return; //obviously wasn't for me...
    //Do end stuffs here.
    uint32_t duration = time(NULL) - (uint32_t)client["time"];
    if(checkDB(dbconn,dbconstring)) {
        try {
            work dbwork(*dbconn);
            stringstream query;
            query << "UPDATE stream_hits SET duration=CAST('" << duration << " seconds' as interval) WHERE id=" << (string)client["dbid"];
            dbwork.exec(query.str());
            dbwork.commit();
        } catch(const std::exception &e) {
            FATAL("%s",e.what());
            
        }
    }
    connections.RemoveAt(id);
}

bool RTMPAppProtocolHandler::ProcessInvokeCloseStream(BaseRTMPProtocol *pFrom,Variant &request) {
    //this is called both when a publisher exits, and when a client exits...
    if(connections[pFrom->GetId()] != V_NULL) {
        client_close(pFrom->GetId());
        INFO("CLOSING CLIENT! It warned me :-)");
    }
    INFO("Stream closing!!!");
    return BaseRTMPAppProtocolHandler::ProcessInvokeCloseStream(pFrom,request);
}

//This allows us to catch any clients that close too forcibly for the above:
void RTMPAppProtocolHandler::UnRegisterProtocol(BaseProtocol *pProtocol) {
    if(connections[pProtocol->GetId()] != V_NULL) {
        client_close(pProtocol->GetId());
        INFO("CLOSING CLIENT! It didn't warn me! :-(");
    }
    BaseRTMPAppProtocolHandler::UnRegisterProtocol(pProtocol);
}

bool RTMPAppProtocolHandler::ProcessInvoke(BaseRTMPProtocol *pFrom, Variant &request) {
    string command = M_INVOKE_FUNCTION(request);
    INFO("INVOKE: %s",command.c_str());
    return BaseRTMPAppProtocolHandler::ProcessInvoke(pFrom,request);
}

bool RTMPAppProtocolHandler::ProcessInvokePublish(BaseRTMPProtocol *pFrom,Variant &request) {
	string streamNameFull = M_INVOKE_PARAM(request, 1);
	size_t len = streamNameFull.find('?');
	string streamName;
	if(len != string::npos)
		streamName = streamNameFull.substr(0,len);
	else
		streamName = streamNameFull;
        bool allowed = false;
        if(checkDB(dbconn,dbconstring)) {
            try {
                work dbwork(*dbconn);
                result res = dbwork.exec("SELECT * from red5_stream_sources WHERE ip='" + getIP(pFrom) + "'");
                dbwork.commit();
                if(res.size() != 0) {
                    //ip address is valid
                    work moredbwork(*dbconn);
                    result nextres = moredbwork.exec("SELECT * FROM red5_streams WHERE shortname='" + streamName + "' AND enabled = true");
                    moredbwork.commit();
                    if(nextres.size() != 0) {
                        //stream name is valid.
                        work yetmoredbwork(*dbconn);
                        yetmoredbwork.exec("UPDATE red5_streams SET is_current=true WHERE shortname='" + streamName + "'");
                        yetmoredbwork.commit();
                        allowed = true;
                    }
                    else {
                        INFO("That stream (%s) isn't allowed here!",streamName.c_str());
                    }
                }
                else {
                    INFO("That host (%s) isn't allowed to stream here!",getIP(pFrom).c_str());
                }
            } catch(const std::exception &e) {
                FATAL("%s",e.what());
                // false;//something went wrong, so be mean as a safe option.
            }
        }
        if(allowed) {
            INFO("Publishing started on stream %s",streamName.c_str());
            return BaseRTMPAppProtocolHandler::ProcessInvokePublish(pFrom,request);
        }
        else {
            return false;
        }
}



bool RTMPAppProtocolHandler::ProcessGetAvailableFlvs(BaseRTMPProtocol *pFrom, Variant &request) {
	Variant parameters;
	parameters.PushToArray(Variant());
	parameters.PushToArray(Variant());

	vector<string> files;
	if (!listFolder(_configuration[CONF_APPLICATION_MEDIAFOLDER],
			files)) {
		FATAL("Unable to list folder %s",
				STR(_configuration[CONF_APPLICATION_MEDIAFOLDER]));
		return false;
	}

	string file, name, extension;
	size_t normalizedMediaFolderSize = 0;
	string mediaFolderPath = normalizePath(_configuration[CONF_APPLICATION_MEDIAFOLDER], "");
	if ((mediaFolderPath != "") && (mediaFolderPath[mediaFolderPath.size() - 1] == PATH_SEPARATOR))
		normalizedMediaFolderSize = mediaFolderPath.size();
	else
		normalizedMediaFolderSize = mediaFolderPath.size() + 1;

	FOR_VECTOR_ITERATOR(string, files, i) {
		file = VECTOR_VAL(i).substr(normalizedMediaFolderSize);

		splitFileName(file, name, extension);
		extension = lowerCase(extension);

		if (extension != MEDIA_TYPE_FLV
				&& extension != MEDIA_TYPE_MP3
				&& extension != MEDIA_TYPE_MP4
				&& extension != MEDIA_TYPE_M4A
				&& extension != MEDIA_TYPE_M4V
				&& extension != MEDIA_TYPE_MOV
				&& extension != MEDIA_TYPE_F4V
				&& extension != MEDIA_TYPE_TS
				&& extension != MEDIA_TYPE_NSV)
			continue;
		string flashName = "";
		if (extension == MEDIA_TYPE_FLV) {
			flashName = name;
		} else if (extension == MEDIA_TYPE_MP3) {
			flashName = extension + ":" + name;
		} else if (extension == MEDIA_TYPE_NSV) {
			flashName = extension + ":" + name + "." + extension;
		} else {
			if (extension == MEDIA_TYPE_MP4
					|| extension == MEDIA_TYPE_M4A
					|| extension == MEDIA_TYPE_M4V
					|| extension == MEDIA_TYPE_MOV
					|| extension == MEDIA_TYPE_F4V) {
				flashName = MEDIA_TYPE_MP4":" + name + "." + extension;
			} else {
				flashName = extension + ":" + name + "." + extension;
			}
		}

		parameters[(uint32_t) 1].PushToArray(flashName);
	}

	map<uint32_t, BaseStream *> allInboundStreams =
			GetApplication()->GetStreamsManager()->FindByType(ST_IN_NET, true);

	FOR_MAP(allInboundStreams, uint32_t, BaseStream *, i) {
		parameters[(uint32_t) 1].PushToArray(MAP_VAL(i)->GetName());
	}

	Variant message = GenericMessageFactory::GetInvoke(3, 0, 0, false, 0,
			"SetAvailableFlvs", parameters);

	return SendRTMPMessage(pFrom, message);
}

bool RTMPAppProtocolHandler::ProcessInsertMetadata(BaseRTMPProtocol *pFrom, Variant &request) {
	NYIR;
}
#endif /* HAS_PROTOCOL_RTMP */

