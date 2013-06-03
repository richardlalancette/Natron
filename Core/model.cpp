//  Powiter
//
//  Created by Alexandre Gauthier-Foichat on 06/12
//  Copyright (c) 2013 Alexandre Gauthier-Foichat. All rights reserved.
//  contact: immarespond at gmail dot com
#include <QtCore/QMutex>
#include <QtCore/QDir>
#include <cassert>
#include <cstdio>
#include "Core/model.h"
#include "Core/outputnode.h"
#include "Superviser/controler.h"
#include "Core/hash.h"
#include "Core/node.h"
#include "Core/channels.h"
#include "Reader/Reader.h"
#include "Core/viewerNode.h"
#include "Gui/mainGui.h"
#include "Core/inputnode.h"
#include "Gui/GLViewer.h"
#include "Core/VideoEngine.h"
#include "Core/displayFormat.h"
#include "Core/settings.h"
#include "Reader/Read.h"
#include "Reader/readExr.h"
#include "Reader/readffmpeg.h"
#include "Reader/readQt.h"
using namespace std;
Model::Model(): ctrl(0),vengine(0),_mutex(0),_powiterSettings(0)
{
    _mutex = new QMutex;
    _powiterSettings = new Settings;

    loadPluginsAndInitNameList();
    loadBuiltinPlugins();
    loadReadPlugins();
    displayLoadedPlugins();

}
void Model::setControler(Controler* ctrl){
    this->ctrl=ctrl;
    vengine = new VideoEngine(ctrl->getGui()->viewer_tab->viewer,this,_mutex);
    connect(this,SIGNAL(vengineNeeded(int)),vengine,SLOT(startEngine(int)));
    formatNames.push_back(string("PC_Video"));
    formatNames.push_back(string("NTSC"));
    formatNames.push_back(string("PAL"));
    formatNames.push_back(string("HD"));
    formatNames.push_back(string("NTSC_16:9"));
    formatNames.push_back(string("PAL_16:9"));
    formatNames.push_back(string("1K_Super_35(full-ap)"));
    formatNames.push_back(string("1K_Cinemascope"));
    formatNames.push_back(string("2K_Super_35(full-ap)"));
    formatNames.push_back(string("2K_Cinemascope"));
    formatNames.push_back(string("4K_Super_35(full-ap)"));
    formatNames.push_back(string("4K_Cinemascope"));
    formatNames.push_back(string("square_256"));
    formatNames.push_back(string("square_512"));
    formatNames.push_back(string("square_1K"));
    formatNames.push_back(string("square_2K"));
    
    resolutions.push_back(new Imath::V3f(640,480,1)); // pc video
    resolutions.push_back(new Imath::V3f(720,486,0.91)); // ntsc
    resolutions.push_back(new Imath::V3f(720,576,1.09)); // pal
    resolutions.push_back(new Imath::V3f(1920,1080,1)); //hd
    resolutions.push_back(new Imath::V3f(720,486,1.21)); // ntsc 16 9
    resolutions.push_back(new Imath::V3f(720,576,1.46)); //pal 16 9 
    resolutions.push_back(new Imath::V3f(1024,778,1)); // 1k super 35 full ap
    resolutions.push_back(new Imath::V3f(914,778,2)); //1k cinemascope
    resolutions.push_back(new Imath::V3f(2048,1556,1)); // 2k super 35 full ap
    resolutions.push_back(new Imath::V3f(1828,1556,2));//2k cinemascope
    resolutions.push_back(new Imath::V3f(4096,3112,1)); // 4k super 35 full ap
    resolutions.push_back(new Imath::V3f(3656,3112,2)); // 4K cinemascope
    resolutions.push_back(new Imath::V3f(256,256,1)); // square 256
    resolutions.push_back(new Imath::V3f(512,512,1));// square 512
    resolutions.push_back(new Imath::V3f(1024,1024,1));// square 1K
    resolutions.push_back(new Imath::V3f(2048,2048,1));// square 2K
    
    
    assert(formatNames.size() == resolutions.size());
    for(int i =0;i<formatNames.size();i++){
        Imath::V3f *v= resolutions[i];
        Format* _frmt = new Format(0,0,v->x,v->y,formatNames[i],v->z);
        addFormat(_frmt);
    }
    
}


Model::~Model(){
   
    vengine->abort();
    foreach(PluginID* p,plugins) delete p;
    foreach(CounterID* c,counters) delete c;
    foreach(Imath::V3f* p, resolutions) delete p;
    foreach(Format* f,formats_list) delete f;
    plugins.clear();
    counters.clear();
    allNodes.clear();
    formatNames.clear();
    resolutions.clear();
    formats_list.clear();
    nodeNameList.clear();
    delete _powiterSettings;
    delete _mutex;
    delete vengine;
}



void Model::loadPluginsAndInitNameList(){ // parses Powiter directory to find classes who inherit Node and adds them to the nodeList
    QDir d(PLUGINS_PATH);
    if (d.isReadable())
    {
        QStringList filters;
#ifdef __POWITER_WIN32__
        filters << "*.dll";
#elif defined(__POWITER_OSX__)
        filters << "*.dylib";
#elif defined(__POWITER_LINUX__)
        filters << "*.so";
#endif
        d.setNameFilters(filters);
		QStringList fileList = d.entryList();
        for(int i = 0 ; i < fileList.size() ;i ++)
        {
            QString filename = fileList.at(i);
            if(filename.contains(".dll") || filename.contains(".dylib") || filename.contains(".so")){
                QString className;
                int index = filename.size() -1;
                while(filename.at(index) != QChar('.')) index--;
                className = filename.left(index);
                nodeNameList.append(QString(className));

#ifdef __POWITER_WIN32__
                HINSTANCE lib;
                string dll;
                dll.append(PLUGINS_PATH);
                dll.append(className.toStdString());
                dll.append(".dll");
                lib=LoadLibrary(dll.c_str());
                if(lib==NULL){
                    cout << " couldn't open library " << qPrintable(className) << endl;
                }else{
                    PluginID* plugin=new PluginID(lib,className.toStdString());
                    plugins.push_back(plugin);
                }
                
#elif defined(__POWITER_UNIX__)
                string dll;
                dll.append(PLUGINS_PATH);
                dll.append(className.toStdString());
#ifdef __POWITER_OSX__
                dll.append(".dylib");
#elif defined(__POWITER_LINUX__)
                dll.append(".so");
#endif
                void* lib=dlopen(dll.c_str(),RTLD_LAZY);
                if(!lib){
                    cout << " couldn't open library " << qPrintable(className) << endl;
                }
                else{
                    PluginID* plugin=new PluginID((void*)lib,className.toStdString());
                    plugins.push_back(plugin);
                    
                }
#endif
            }else{
                continue;
            }
        }
    }
}

std::string Model::getNextWord(string str){
    string res;
    int i=0;
    while(i!=str.size() && str[i]==' '){
        i++;
    }
    while(i!=str.size() && str[i]!=' ' && str[i]!=':'){
        res.push_back(str[i]);
        i++;
    }
    return res;
}

std::string Model::removePrefixSpaces(std::string str){
    string res;
    int i=0;
    while(i!=str.size() && str[i]==' '){
        i++;
    }
    res=str.substr(i,str.size());
    return res;
    
}



void Model::setVideoEngineRequirements(OutputNode *output){
    vengine->getCurrentDAG().resetAndSort(output);
    vengine->changeTreeVersion();
}

UI_NODE_TYPE Model::initCounterAndGetDescription(Node*& node){
    bool found=false;
    foreach(CounterID* counter,counters){
        string tmp(counter->second);
        string nodeName = node->className();
        if(tmp==nodeName){
            (counter->first)++;
            found=true;
            QString str;
            str.append(nodeName.c_str());
            str.append("_");
            char c[50];
            sprintf(c,"%d",counter->first);
            str.append(c);
            node->setName(str);
        }
    }
    if(!found){
        CounterID* count=new CounterID(1,node->className());
        
        counters.push_back(count);
        QString str;
        str.append(node->className().c_str());
        str.append("_");
        char c[50];
        sprintf(c,"%d",count->first);
        str.append(c);
        node->setName(str);
    }
	node->setOutputNb();
    allNodes.push_back(node);
    string outputNodeSymbol="OutputNode";
    string inputNodeSymbol="InputNode";
    string flowOpSymbol="FlowOperator";
    string imgOperatorSymbol="ImgOperator";
    string desc =node->description();
    if(desc==outputNodeSymbol){
        return OUTPUT;
    }
    else if(desc==inputNodeSymbol){
		return INPUT_NODE;
    }else if(desc==imgOperatorSymbol || desc==flowOpSymbol){
        return OPERATOR;
    }
    return UNDEFINED;
}

UI_NODE_TYPE Model::createNode(Node *&node,QString& name,QMutex* m){
	if(name=="Reader"){
		UI_NODE_TYPE type;
		node=new Reader(node,ctrl->getGui()->viewer_tab->viewer,vengine->getViewerCache());
        node->setMutex(m);
        node->_inputs();
		type=initCounterAndGetDescription(node);
		return type;
	}else if(name =="Viewer"){
		UI_NODE_TYPE type;
		node=new Viewer(node,ctrl->getGui()->viewer_tab->viewer);
        node->setMutex(m);
        node->_inputs();
		type=initCounterAndGetDescription(node);
		return type;
	}else{
        
        UI_NODE_TYPE type=UNDEFINED;
        
		foreach(PluginID* pair,plugins){
			string str(pair->second);
            
			if(str==name.toStdString()){
				
				string handleName;
				handleName.append("Build");
				handleName.append(pair->second);
#ifdef __POWITER_WIN32__
                
				NodeBuilder builder=(NodeBuilder)GetProcAddress(pair->first,handleName.c_str());
				if(builder!=NULL){
					node=builder(node);
                    node->setMutex(m);
                    node->_inputs();
					type=initCounterAndGetDescription(node);
                    
                    
				}else{
					cout << "RunTime: couldn't call " << handleName.c_str() << endl;
				}
                
#elif defined(__POWITER_UNIX__)
				NodeBuilder builder=(NodeBuilder)dlsym(pair->first,handleName.c_str());
				if(builder!=NULL){
					node=builder(node);
					type=initCounterAndGetDescription(node);
                    
				}else{
					cout << "RunTime: couldn't call " << handleName.c_str() << endl;
				}
                
#endif
                
				return type;
			}
            
		}
	}
    return UNDEFINED;
    
    
    
}
// in the future, display the plugins loaded on the loading wallpaper
void Model::displayLoadedPlugins(){
    int i=0;
    foreach(PluginID* plugin,plugins){
        i++;
        cout << "Plugin:  " << plugin->second << endl;
    }
    cout  << i << " plugin(s) loaded." << endl;
}


void Model::addFormat(Format* frmt){formats_list.push_back(frmt);}

Format* Model::findExistingFormat(int w, int h, double pixel_aspect){

	for(int i =0;i< formats_list.size();i++){
		Format* frmt = formats_list[i];
		if(frmt->w() == w && frmt->h() == h && frmt->pixel_aspect()==pixel_aspect){
			return frmt;
		}
	}
	return NULL;
}


void Model::loadReadPlugins(){
    QDir d(PLUGINS_PATH);
    if (d.isReadable())
    {
        QStringList filters;
#ifdef __POWITER_WIN32__
        filters << "*.dll";
#elif defined(__POWITER_OSX__)
        filters << "*.dylib";
#elif defined(__POWITER_LINUX__)
        filters << "*.so";
#endif
        d.setNameFilters(filters);
		QStringList fileList = d.entryList();
        for(int i = 0 ; i < fileList.size() ;i ++)
        {
            QString filename = fileList.at(i);
            if(filename.contains(".dll") || filename.contains(".dylib") || filename.contains(".so")){
                QString className;
                int index = filename.size() -1;
                while(filename.at(index) != QChar('.')) index--;
                className = filename.left(index);
                PluginID* plugin = 0;
#ifdef __POWITER_WIN32__
                HINSTANCE lib;
                string dll;
                dll.append(PLUGINS_PATH);
                dll.append(className.toStdString());
                dll.append(".dll");
                lib=LoadLibrary(dll.c_str());
                if(lib==NULL){
                    cout << " couldn't open library " << qPrintable(className) << endl;
                }else{
                    // successfully loaded the library, we create now an instance of the class
                    //to find out the extensions it can decode and the name of the decoder
                    ReadBuilder builder=(ReadBuilder)GetProcAddress(lib,"BuildRead");
                    if(builder!=NULL){
                        Read* read=builder(NULL);
                        std::vector<std::string> extensions = read->fileTypesDecoded();
                        std::string decoderName = read->decoderName();
                        plugin = new PluginID((HINSTANCE)builder,decoderName.c_str());
                        for (U32 i = 0 ; i < extensions.size(); i++) {
                            readPlugins.insert(make_pair(extensions[i],plugin));
                        }
                        delete read;
                        
                    }else{
                        cout << "RunTime: couldn't call " << "BuildRead" << endl;
                        continue;
                    }
                    
                }
                
#elif defined(__POWITER_UNIX__)
                string dll;
                dll.append(PLUGINS_PATH);
                dll.append(className.toStdString());
#ifdef __POWITER_OSX__
                dll.append(".dylib");
#elif defined(__POWITER_LINUX__)
                dll.append(".so");
#endif
                void* lib=dlopen(dll.c_str(),RTLD_LAZY);
                if(!lib){
                    cout << " couldn't open library " << qPrintable(className) << endl;
                }
                else{
                    // successfully loaded the library, we create now an instance of the class
                    //to find out the extensions it can decode and the name of the decoder
                    ReadBuilder builder=(ReadBuilder)dlsym(lib,"BuildRead");
                    if(builder!=NULL){
                        Read* read=builder(NULL);
                        std::vector<std::string> extensions = read->fileTypesDecoded();
                        std::string decoderName = read->decoderName();
                        plugin = new PluginID((void*)builder,decoderName.c_str());
                        for (U32 i = 0 ; i < extensions.size(); i++) {
                            readPlugins.insert(make_pair(extensions[i],plugin));
                        }
                        delete read;
                        
                    }else{
                        cout << "RunTime: couldn't call " << "BuildRead" << endl;
                        continue;
                    }
                }
#endif
            }else{
                continue;
            }
        }
    }
    loadBuiltinReads();
    
    std::map<std::string, PluginID*> defaultMapping;
    for (ReadPluginsIterator it = readPlugins.begin(); it!=readPlugins.end(); it++) {
        if(it->first == "exr" && it->second->second == "OpenEXR"){
            defaultMapping.insert(*it);
        }else if (it->first == "dpx" && it->second->second == "FFmpeg"){
            defaultMapping.insert(*it);
        }else if((it->first == "jpg" ||
                  it->first == "bmp" ||
                  it->first == "jpeg"||
                  it->first == "gif" ||
                  it->first == "png" ||
                  it->first == "pbm" ||
                  it->first == "pgm" ||
                  it->first == "ppm" ||
                  it->first == "xbm" ||
                  it->first == "xpm") && it->second->second == "QImage (Qt)"){
            defaultMapping.insert(*it);
            
        }
    }
    _powiterSettings->_readersSettings.fillMap(defaultMapping);
}

void Model::displayLoadedReads(){
    std::multimap<std::string,PluginID*>::iterator it = readPlugins.begin();
    for (; it!=readPlugins.end(); it++) {
        cout << it->second->second << " : " << it->first << endl;
    }
}

void Model::loadBuiltinReads(){
    Read* readExr = ReadExr::BuildRead(NULL);
    std::vector<std::string> extensions = readExr->fileTypesDecoded();
    std::string decoderName = readExr->decoderName();
#ifdef __POWITER_WIN32__
	 PluginID *EXRplugin = new PluginID((HINSTANCE)&ReadExr::BuildRead,decoderName.c_str());
#else
	 PluginID *EXRplugin = new PluginID((void*)&ReadExr::BuildRead,decoderName.c_str());
#endif
   
    for (U32 i = 0 ; i < extensions.size(); i++) {
        readPlugins.insert(make_pair(extensions[i],EXRplugin));
    }
    delete readExr;
    
    Read* readQt = ReadQt::BuildRead(NULL);
    extensions = readQt->fileTypesDecoded();
    decoderName = readQt->decoderName();
#ifdef __POWITER_WIN32__
	PluginID *Qtplugin = new PluginID((HINSTANCE)&ReadQt::BuildRead,decoderName.c_str());
#else
	PluginID *Qtplugin = new PluginID((void*)&ReadQt::BuildRead,decoderName.c_str());
#endif
    for (U32 i = 0 ; i < extensions.size(); i++) {
        readPlugins.insert(make_pair(extensions[i],Qtplugin));
    }
    delete readQt;
    
    Read* readFfmpeg = ReadFFMPEG::BuildRead(NULL);
    extensions = readFfmpeg->fileTypesDecoded();
    decoderName = readFfmpeg->decoderName();
#ifdef __POWITER_WIN32__
	PluginID *FFMPEGplugin = new PluginID((HINSTANCE)&ReadFFMPEG::BuildRead,decoderName.c_str());
#else
	PluginID *FFMPEGplugin = new PluginID((void*)&ReadFFMPEG::BuildRead,decoderName.c_str());
#endif
    for (U32 i = 0 ; i < extensions.size(); i++) {
        readPlugins.insert(make_pair(extensions[i],FFMPEGplugin));
    }
    delete readFfmpeg;
    
}
void Model::loadBuiltinPlugins(){
    // these  are built-in nodes
	nodeNameList.append("Reader");
	nodeNameList.append("Viewer");
}