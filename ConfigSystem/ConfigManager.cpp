#include "ConfigManager.h"
#include "ConfigStorageManager.h"
#include "Configurable.h"

#include <iostream>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/foreach.hpp>
#include <vector>

using ConfigSystem::ConfigStorageManager;


namespace ConfigSystem
{
    
    ConfigManager::ConfigManager(std::string configName)
    {
    	CONFIGSYS_DEBUG_CALLS;

        // set initial values
        _configStore    = NULL;
        _currConfigTree = NULL;
        _configObjects  = std::vector<Configurable*>();

        _configStore = new ConfigStorageManager();
    	loadConfiguration(configName);
    }

    ConfigManager::~ConfigManager()
    {
        CONFIGSYS_DEBUG_CALLS;

        delete _configStore     ; _configStore     = NULL;
        delete _currConfigTree  ; _currConfigTree  = NULL;
    }
    

    bool ConfigManager::loadConfiguration(std::string configName)
    {
        bool loaded = _configStore->loadConfig(_currConfigTree, configName);

        // If loading failed, output an error message
        if(!loaded)
        {
            std::cout << "ConfigManager::ConfigManager(std::string): "
                      << "Could not load configuration '"
                      << configName << "'."
                      << std::endl;
        }
        else
        {
            // Send the new configuration to the configObjects
            // #warning Should occur within updateConfiguration()
            reconfigureConfigObjects();
        }
        
        return loaded;
    }
    
    bool ConfigManager::saveConfiguration(std::string configName)
    {
        bool saved = _configStore->saveConfig(_currConfigTree, configName);

        // If saving failed, output an error message
        if(!saved)
        {
            std::cout << "ConfigManager::ConfigManager(std::string): "
                      << "Could not save configuration '"
                      << configName << "'."
                      << std::endl;
        }

        return saved;
    }


    void ConfigManager::updateConfiguration()
    {
        // Update all ConfigObjects whose configurations have been outdated.
        updateConfigObjects();
    }


    bool ConfigManager::setConfigObjects(std::vector<Configurable*> configObjects)
    {
        _configObjects = configObjects;
        reconfigureConfigObjects();
        return true;
    }

    bool ConfigManager::addConfigObject(Configurable* configObject)
    {
        if(configObject == NULL) return false;
        _configObjects.push_back(configObject);

        // Should refactor this method, and reconfigureConfigObjects() to call
        // a 'reconfigureConfigObject' method.
        configObject->loadConfig();
        configObject->setConfigAsRecent();
        return true;
    }
    
    void ConfigManager::reconfigureConfigObjects()
    {
        BOOST_FOREACH(Configurable* c, _configObjects)
        {
            reconfigureConfigObject(c);
        }
    }

    void ConfigManager::reconfigureConfigObject(Configurable* c)
    {
        if(c == NULL) return;
        c->loadConfig();
        c->setConfigAsRecent();
    }


    void ConfigManager::updateConfigObjects()
    {
        BOOST_FOREACH(Configurable* c, _configObjects)
        {
            if(c == NULL) continue;

            if(c->isConfigOutdated())
            {
                c->updateConfig();
                c->setConfigAsRecent();
            }
        }
    }
    
    void ConfigManager::markConfigObjects(
            const std::string &paramPath,
            const std::string &paramName
            )
    {
        BOOST_FOREACH(Configurable* c, _configObjects)
        {
            if(c == NULL) continue;
            
            //! skip string comparison if already marked
            if(c->isConfigOutdated()) continue;
            
            //! Check whether the given path is on c's base path
            // Note: Should use some clever data structure to speed this up.
            // (such a data structure would be initialised within 
            // ConfigManager::setConfigObjects(...))
            if(boost::starts_with(paramPath, c->getConfigBasePath()))
                c->setConfigAsOutdated();
        }
    }
    
    
    template<typename T>
    bool ConfigManager::createParam(
        const std::string &paramPath,
        const std::string &paramName,
        T initialValue
        )
    {
        CONFIGSYS_DEBUG_CALLS;

        //! Get the relevant parameter from the ConfigTree
        if(_currConfigTree->checkParam(paramPath, paramName))
        {
            std::cout << "ConfigManager::createParam(...): "
                      << paramPath << "." << paramName << " already exists."
                      << std::endl;
            return false;
        }

        //! Create the new ConfigParameter
        ConfigParameter cp(vt_none);
        cp = ConfigParameter(initialValue);

        //! Store the new parameter into the tree
        if(!_currConfigTree->storeParam(paramPath, paramName, cp)) return false;
        
        //! Request update of configObjects that depend on this parameter
        //! (these actually might exist)
        markConfigObjects(paramPath, paramName);

        return true;
    }
    // Define allowed template parameters (using explicit template instantiations).
    template bool ConfigManager::createParam<long> (
        const std::string &paramPath, const std::string &paramName,
        long initialValue
        );
    template bool ConfigManager::createParam<double> (
        const std::string &paramPath, const std::string &paramName,
        double initialValue
        );
    template bool ConfigManager::createParam<std::string> (
        const std::string &paramPath, const std::string &paramName,
        std::string initialValue
        );
    template bool ConfigManager::createParam<std::vector<long> > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<long> initialValue
        );
    template bool ConfigManager::createParam<std::vector<std::vector<long> > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<long> > initialValue
        );
    template bool ConfigManager::createParam<std::vector<std::vector<std::vector<long> > > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<std::vector<long> > > initialValue
        );
    template bool ConfigManager::createParam<std::vector<double> > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<double>  initialValue
        );
    template bool ConfigManager::createParam<std::vector<std::vector<double> > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<double> >  initialValue
        );
    template bool ConfigManager::createParam<std::vector<std::vector<std::vector<double> > > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<std::vector<double> > > initialValue
        );



    bool ConfigManager::deleteParam(
            const std::string &paramPath,
            const std::string &paramName
            )
    {
        CONFIGSYS_DEBUG_CALLS;
        
        ConfigParameter cp(vt_none);
        
        //OLD CODE
        //Checking if parameter exists, locking won't work as cp is not returned from anything (compiler
        //error). 
        //if(!_currConfigTree->checkParam(paramPath, paramName)) return false;
        //else if(cp.isLocked()) return false;
        
        //Did you mean getParam? If the parameter doesn't exist, it should return false from that function,
        //albeit a bit cryptic.
        if(!_currConfigTree->getParam(paramPath, paramName, cp)) return false;
        else if(cp.isLocked()) return false;
		
        if(!_currConfigTree->deleteParam(paramPath, paramName)) return false;

        return true;
    }



    bool ConfigManager::lockParam(
            const std::string &paramPath,
            const std::string &paramName
            )
    {
        CONFIGSYS_DEBUG_CALLS;
        //! Get the relevant parameter from the ConfigTree
        ConfigParameter cp(vt_none);
        if(!_currConfigTree->getParam(paramPath, paramName, cp)) return false;
        
        // The parameter is already locked
        if(cp.isLocked()) return true;
        
        //! Lock the parameter
        cp.setLocked(true);
        
        //! Store the modified parameter back into the tree
        if(!_currConfigTree->storeParam(paramPath, paramName, cp)) return false;

        return true;
    }

    bool ConfigManager::unlockParam(
            const std::string &paramPath,
            const std::string &paramName
            )
    {
        CONFIGSYS_DEBUG_CALLS;
        //! Get the relevant parameter from the ConfigTree
        ConfigParameter cp(vt_none);
        if(!_currConfigTree->getParam(paramPath, paramName, cp)) return false;
        
        // The parameter is not locked
        if(!cp.isLocked()) return true;
        
        //! Unlock the parameter
        cp.setLocked(false);
        
        //! Store the modified parameter back into the tree
        if(!_currConfigTree->storeParam(paramPath, paramName, cp)) return false;

        return true;
    }


    bool ConfigManager::setParamDescription(
        const std::string &paramPath,
        const std::string &paramName,
        const std::string &paramDesc
        )
    {
        CONFIGSYS_DEBUG_CALLS;
        //! Get the relevant parameter from the ConfigTree
        ConfigParameter cp(vt_none);
        if(!_currConfigTree->getParam(paramPath, paramName, cp)) return false;
        
        // If the parameter is locked, you can't set the description
        // (is important that good description aren't accidentally deleted by
        // people who couldn't/wouldn't replace them)
        if(cp.isLocked()) return false;
        
        //! Unlock the parameter
        cp.setDescription(paramDesc);
        
        //! Store the modified parameter back into the tree
        if(!_currConfigTree->storeParam(paramPath, paramName, cp)) return false;

        return true;
    }


    template<typename T>
    bool ConfigManager::readValue (
        const std::string &paramPath,
        const std::string &paramName,
        T &data
        )
    {
        CONFIGSYS_DEBUG_CALLS;
        ConfigParameter cp(vt_none);
        if(!_currConfigTree->getParam(paramPath, paramName, cp)) return false;
        return cp.getValue(data);
    }


    // New interface (using explicit template instantiations).
    template bool ConfigManager::readValue<long> (
        const std::string &paramPath, const std::string &paramName,
        long &data
        );
    template bool ConfigManager::readValue<double> (
        const std::string &paramPath, const std::string &paramName,
        double &data
        );
    template bool ConfigManager::readValue<std::string> (
        const std::string &paramPath, const std::string &paramName,
        std::string &data
        );
    template bool ConfigManager::readValue<std::vector<long> > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<long> &data
        );
    template bool ConfigManager::readValue<std::vector<std::vector<long> > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<long> > &data
        );
    template bool ConfigManager::readValue<std::vector<std::vector<std::vector<long> > > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<std::vector<long> > > &data
        );
    template bool ConfigManager::readValue<std::vector<double> > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<double>  &data
        );
    template bool ConfigManager::readValue<std::vector<std::vector<double> > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<double> >  &data
        );
    template bool ConfigManager::readValue<std::vector<std::vector<std::vector<double> > > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<std::vector<double> > > &data
        );



    template<typename T>
    bool ConfigManager::storeValue(
       const std::string &paramPath,
       const std::string &paramName,
       T data)
    {
        CONFIGSYS_DEBUG_CALLS;
        //! Get the relevant parameter from the ConfigTree
        ConfigParameter cp(vt_none);
        if(!_currConfigTree->getParam(paramPath, paramName, cp)) return false;
        
        //! Set the new value
        if(!cp.setValue(data)) return false; 
        
        //! Store the modified parameter back into the tree
        if(!_currConfigTree->storeParam(paramPath, paramName, cp)) return false;
        
        //! Request update of configObjects that depend on this parameter
        markConfigObjects(paramPath, paramName);

        return true;
    }
    
    // New interface (using explicit template instantiations).
    template bool ConfigManager::storeValue<long> (
        const std::string &paramPath, const std::string &paramName,
        long data
        );
    template bool ConfigManager::storeValue<double> (
        const std::string &paramPath, const std::string &paramName,
        double data
        );
    template bool ConfigManager::storeValue<std::string> (
        const std::string &paramPath, const std::string &paramName,
        std::string data
        );
    template bool ConfigManager::storeValue<std::vector<long> > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<long> data
        );
    template bool ConfigManager::storeValue<std::vector<std::vector<long> > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<long> > data
        );
    template bool ConfigManager::storeValue<std::vector<std::vector<std::vector<long> > > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<std::vector<long> > > data
        );
    template bool ConfigManager::storeValue<std::vector<double> > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<double>  data
        );
    template bool ConfigManager::storeValue<std::vector<std::vector<double> > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<double> >  data
        );
    template bool ConfigManager::storeValue<std::vector<std::vector<std::vector<double> > > > (
        const std::string &paramPath, const std::string &paramName,
        std::vector<std::vector<std::vector<double> > > data
        );




    bool ConfigManager::readRange(const string &paramPath, 
                                  const string &paramName, 
                                  ConfigRange<double> &range)
    {
        CONFIGSYS_DEBUG_CALLS;

        ConfigParameter cp(vt_none);
        if(!_currConfigTree->getParam(paramPath, paramName, cp)) return false;
        return cp.getRange(range);
    }
    bool ConfigManager::readRange  (const string &paramPath, 
                                    const string &paramName, 
                                    ConfigRange<long> &range)
    {
        CONFIGSYS_DEBUG_CALLS;

        ConfigParameter cp(vt_none);
        if(!_currConfigTree->getParam(paramPath, paramName, cp)) return false;
        return cp.getRange(range);
    }

    bool ConfigManager::storeRange(const string &paramPath, 
                                   const string &paramName, 
                                   ConfigRange<double> &range)
    {
        CONFIGSYS_DEBUG_CALLS;
        //! Get the relevant parameter from the ConfigTree
        ConfigParameter cp(vt_none);
        if(!_currConfigTree->getParam(paramPath, paramName, cp)) return false;
        if(!cp.setRange(range)) return false; //!< Set the new value
        //! Store the modified parameter back into the tree
        return _currConfigTree->storeParam(paramPath, paramName, cp);
    }
    bool ConfigManager::storeRange (const string &paramPath, 
                                    const string &paramName, 
                                    ConfigRange<long> &range)
    {
        CONFIGSYS_DEBUG_CALLS;
        //! Get the relevant parameter from the ConfigTree
        ConfigParameter cp(vt_none);
        if(!_currConfigTree->getParam(paramPath, paramName, cp)) return false;
        if(!cp.setRange(range)) return false; //!< Set the new value
        //! Store the modified parameter back into the tree
        return _currConfigTree->storeParam(paramPath, paramName, cp);
    }
}
