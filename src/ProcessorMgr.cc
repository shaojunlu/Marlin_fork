#include "marlin/ProcessorMgr.h"
#include "marlin/Global.h"
#include "marlin/Exceptions.h"

#include <iostream>
#include <algorithm>

#include "marlin/DataSourceProcessor.h"


namespace marlin{

  ProcessorMgr* ProcessorMgr::_me = 0 ;



  ProcessorMgr* ProcessorMgr::instance() {
  
    if( _me == 0 ) 
      _me = new ProcessorMgr ;

    return _me ;
  }  

  void ProcessorMgr::registerProcessor( Processor* processor ){

    const std::string& name = processor->type()  ;

    if( _map.find( name ) != _map.end() ){

      //     std::cerr << " ProcessorMgr::registerProcessor: processor " <<  name 
      // 	      << " already registered ! " 
      // 	      << std::endl   ;
    
      return ;
    }
    else
    
      _map[ name ] = processor ;

  }
  
  void ProcessorMgr::readDataSource( int numEvents ) {

    for(  ProcessorList::iterator it = _list.begin() ;
	  it != _list.end() ; it++ ){

      DataSourceProcessor* dSP = dynamic_cast<DataSourceProcessor*>( *it ) ; 
      
      if( dSP != 0 )
	dSP->readDataSource( numEvents ) ;

    }
  }


  void  ProcessorMgr::dumpRegisteredProcessors() {

    typedef ProcessorMap::iterator MI ;
    
    std::cout  <<  std::endl 
	       << "   Available processors : " 
	       <<  std::endl 
	       <<  std::endl ;

    for(MI i=_map.begin() ; i!= _map.end() ; i++) {
      i->second->printDescription() ;
    }
  }
  void  ProcessorMgr::dumpRegisteredProcessorsXML() {

    typedef ProcessorMap::iterator MI ;
    
    std::cout  <<  std::endl 
	       << "<marlin>" 
	       <<  std::endl ;

    std::cout  <<  " <execute>" << std::endl 
	       <<  "  <processor name=\"MyAIDAProcessor\"/>" << std::endl
	       <<  "  <processor name=\"MyTestProcessor\"/>  " << std::endl
	       <<  "  <processor name=\"MyLCIOOutputProcessor\"/>  " << std::endl
	       <<  " </execute>" << std::endl
	       << std::endl ;

    std::cout  <<  " <global>" << std::endl 
	       <<  "  <parameter name=\"LCIOInputFiles\"> simjob.slcio </parameter>" << std::endl
	       <<  "  <parameter name=\"MaxRecordNumber\" value=\"5001\" />  " << std::endl
	       <<  "  <parameter name=\"SupressCheck\" value=\"false\" />  " << std::endl
	       <<  " </global>" << std::endl
	       << std::endl ;


    for(MI i=_map.begin() ; i!= _map.end() ; i++) {
      i->second->printDescriptionXML() ;
    }

    std::cout  <<  std::endl 
	       << "</marlin>" 
	       <<  std::endl ;

  }

  

  Processor* ProcessorMgr::getProcessor( const std::string& type ){
    return _map[ type ] ;
  }

  Processor* ProcessorMgr::getActiveProcessor( const std::string& name ) {
    return _activeMap[ name ] ;
  }

  void ProcessorMgr::removeActiveProcessor(  const std::string& name ) {

  
    _list.remove( _activeMap[name] ) ;
    _activeMap.erase( name ) ;

  }
  

  bool ProcessorMgr::addActiveProcessor( const std::string& processorType , 
					 const std::string& processorName , 
					 StringParameters* parameters ,
					 const std::string condition) {

    Processor* processor = getProcessor( processorType ) ;

    if( processor == 0 ) {
      std::cerr << " ProcessorMgr::registerProcessor: unknown processor with type " 
		<<  processorType  << " ! " 	      
		<< std::endl   ;
      return false ;
    }

    
    if( _activeMap.find( processorName ) != _activeMap.end() ){
    
      std::cerr << " ProcessorMgr::addActiveProcessor: processor " <<  processorName 
		<< " already registered ! "
		<< std::endl ;
      return false ;
    
    } else {
    
      Processor* newProcessor = processor->newProcessor() ;
      newProcessor->setName( processorName ) ;
      _activeMap[ processorName ] = newProcessor ;
      _list.push_back( newProcessor ) ;
      _conditions.addCondition( processorName, condition ) ;

      if( parameters != 0 ){
	newProcessor->setParameters( parameters  ) ;
      }
//       // keep a copy of the output processor
//       if( processorType == "LCIOOutputProcessor" ){
// 	_outputProcessor = dynamic_cast<LCIOOutputProcessor*>( newProcessor ) ;
//       }
    }

    return true ;
  }


  void ProcessorMgr::init(){ 

    for_each( _list.begin() , _list.end() , std::mem_fun( &Processor::baseInit ) ) ;
  }

  void ProcessorMgr::processRunHeader( LCRunHeader* run){ 

    for_each( _list.begin() , _list.end() ,  std::bind2nd(  std::mem_fun( &Processor::processRunHeader ) , run ) ) ;
  }   


//   void ProcessorMgr::modifyEvent( LCEvent * evt ) { 
//     if( _outputProcessor != 0 )
//       _outputProcessor->dropCollections( evt ) ;
//   }
  

  void ProcessorMgr::processEvent( LCEvent* evt ){ 

//     static bool isFirstEvent = true ;

//     for_each( _list.begin() , _list.end() ,   std::bind2nd(  std::mem_fun( &Processor::processEvent ) , evt ) ) ;

//     if( Global::parameters->getStringVal("SupressCheck") != "true" ) {
      
//       for_each( _list.begin() , _list.end(), 
// 		std::bind2nd( std::mem_fun( &Processor::check ) , evt ) ) ;
//     }
    
//     if ( isFirstEvent ) {
//       isFirstEvent = false;
//       for_each( _list.begin(), _list.end() , 
// 		std::bind2nd( std::mem_fun( &Processor::setFirstEvent ),isFirstEvent )) ;
//     }

    _conditions.clear() ;

    bool check = ( Global::parameters->getStringVal("SupressCheck") != "true" ) ;


    try{ 

      for( ProcessorList::iterator it = _list.begin() ; it != _list.end() ; ++it ) {
	
	if( _conditions.conditionIsTrue( (*it)->name() ) ) {
	  
	  (*it)->processEvent( evt ) ; 
	  
	  if( check )  (*it)->check( evt ) ;
	  
	  (*it)->setFirstEvent( false ) ;
	}       
      }    
    } catch( SkipEventException& e){

      ++ _skipMap[ e.what() ] ;
    }  
  }
  

  void ProcessorMgr::setProcessorReturnValue( Processor* proc, bool val ) {

    _conditions.setValue( proc->name() , val ) ;

  }
  void ProcessorMgr::setProcessorReturnValue( Processor* proc, bool val, 
					      const std::string& name){
    
    std::string valName = proc->name() + "." + name ;
    _conditions.setValue( valName , val ) ;
  }
  
  void ProcessorMgr::end(){ 

    for_each( _list.begin() , _list.end() ,  std::mem_fun( &Processor::end ) ) ;

//     if( _skipMap.size() > 0 ) {
      std::cout  << " --------------------------------------------------------- " << std::endl
		 << "  Events skipped by processors : " << std::endl ;

      unsigned nSkipped = 0 ;
      for( SkippedEventMap::iterator it = _skipMap.begin() ; it != _skipMap.end() ; it++) {

	std::cout << "       " << it->first << ": \t" <<  it->second << std::endl ;

	nSkipped += it->second ;	
      }
      std::cout  << "  Total: " << nSkipped  << std::endl ;
      std::cout  << " --------------------------------------------------------- "  
		 << std::endl
		 << std::endl ;
//     }

  }

 
} // namespace marlin
