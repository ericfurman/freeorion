#include "ResourcePool.h"

#include <boost/lexical_cast.hpp>

#include "../universe/ObjectMap.h"
#include "../universe/Planet.h"
#include "../util/AppInterface.h"
#include "../util/Logger.h"

//////////////////////////////////////////////////
// ResourcePool
//////////////////////////////////////////////////
ResourcePool::ResourcePool() :
    m_stockpile_object_id(INVALID_OBJECT_ID),
    m_stockpile(0.0),
    m_type(INVALID_RESOURCE_TYPE)
{}

ResourcePool::ResourcePool(ResourceType type) :
    m_stockpile_object_id(INVALID_OBJECT_ID),
    m_stockpile(0.0),
    m_type(type)
{}

const std::vector<int>& ResourcePool::ObjectIDs() const
{ return m_object_ids; }

int ResourcePool::StockpileObjectID() const
{ return m_stockpile_object_id; }

float ResourcePool::Stockpile() const
{ return m_stockpile; }

float ResourcePool::Output() const {
    float retval = 0.0f;
    for (std::map<std::set<int>, float>::const_iterator it = m_connected_object_groups_resource_output.begin();
         it != m_connected_object_groups_resource_output.end(); ++it)
    { retval += it->second; }
    return retval;
}

float ResourcePool::GroupOutput(int object_id) const {
    // find group containing specified object
    for (std::map<std::set<int>, float>::const_iterator it = m_connected_object_groups_resource_output.begin();
         it != m_connected_object_groups_resource_output.end(); ++it)
    {
        const std::set<int>& group = it->first;
        if (group.find(object_id) != group.end())
            return it->second;
    }

    // default return case:
    DebugLogger() << "ResourcePool::GroupOutput passed unknown object id: " << object_id;
    return 0.0f;
}


float ResourcePool::TargetOutput() const {
    float retval = 0.0f;
    for (std::map<std::set<int>, float>::const_iterator it = m_connected_object_groups_resource_target_output.begin();
         it != m_connected_object_groups_resource_target_output.end(); ++it)
    { retval += it->second; }
    return retval;
}

float ResourcePool::GroupTargetOutput(int object_id) const {
    // find group containing specified object
    for (std::map<std::set<int>, float>::const_iterator it = m_connected_object_groups_resource_target_output.begin();
         it != m_connected_object_groups_resource_target_output.end(); ++it)
    {
        const std::set<int>& group = it->first;
        if (group.find(object_id) != group.end())
            return it->second;
    }

    // default return case:
    DebugLogger() << "ResourcePool::GroupTargetOutput passed unknown object id: " << object_id;
    return 0.0f;
}

float ResourcePool::TotalAvailable() const {
    float retval = m_stockpile;
    for (std::map<std::set<int>, float>::const_iterator it = m_connected_object_groups_resource_output.begin();
         it != m_connected_object_groups_resource_output.end(); ++it)
    { retval += it->second; }
    return retval;
}

std::map<std::set<int>, float> ResourcePool::Available() const {
    std::map<std::set<int>, float> retval = m_connected_object_groups_resource_output;

    if (INVALID_OBJECT_ID == m_stockpile_object_id)
        return retval;  // early exit for no stockpile

    // find group that contains the stockpile, and add the stockpile to that group's production to give its availability
    for (std::map<std::set<int>, float>::iterator map_it = retval.begin(); map_it != retval.end(); ++map_it) {
        const std::set<int>& group = map_it->first;
        if (group.find(m_stockpile_object_id) != group.end()) {
            map_it->second += m_stockpile;
            break;  // assuming stockpile is on only one group
        }
    }
    return retval;
}

float ResourcePool::GroupAvailable(int object_id) const {
    DebugLogger() << "ResourcePool::GroupAvailable(" << object_id << ")";
    // available is stockpile + production in this group

    if (m_stockpile_object_id == INVALID_OBJECT_ID)
        return GroupOutput(object_id);

    // need to find if stockpile object is in the requested object's group
    for (std::map<std::set<int>, float>::const_iterator it = m_connected_object_groups_resource_output.begin();
         it != m_connected_object_groups_resource_output.end(); ++it)
    {
        const std::set<int>& group = it->first;
        if (group.find(object_id) != group.end()) {
            // found group for requested object.  is stockpile also in this group?
            if (group.find(m_stockpile_object_id) != group.end())
                return it->second + m_stockpile;    // yes; add stockpile to production to return available
            else
                return it->second;                  // no; just return production as available
        }
    }

    // default return case:
    DebugLogger() << "ResourcePool::GroupAvailable passed unknown object id: " << object_id;
    return 0.0;
}

std::string ResourcePool::Dump() const {
    std::string retval = "ResourcePool type = " + boost::lexical_cast<std::string>(m_type) +
                         " stockpile = " + boost::lexical_cast<std::string>(m_stockpile) +
                         " stockpile_object_id = " + boost::lexical_cast<std::string>(m_stockpile_object_id) +
                         " object_ids: ";
    for (std::vector<int>::const_iterator it = m_object_ids.begin(); it != m_object_ids.end(); ++it)
        retval += boost::lexical_cast<std::string>(*it) + ", ";
    return retval;
}

void ResourcePool::SetObjects(const std::vector<int>& object_ids)
{ m_object_ids = object_ids; }

void ResourcePool::SetConnectedSupplyGroups(const std::set<std::set<int> >& connected_system_groups)
{ m_connected_system_groups = connected_system_groups; }

void ResourcePool::SetStockpileObject(int stockpile_object_id)
{ m_stockpile_object_id = stockpile_object_id; }

void ResourcePool::SetStockpile(float d)
{ m_stockpile = d; }

void ResourcePool::Update() {
    //DebugLogger() << "ResourcePool::Update for type " << boost::lexical_cast<std::string>(m_type);
    // sum production from all ResourceCenters in each group, for resource point type appropriate for this pool
    MeterType meter_type = ResourceToMeter(m_type);
    MeterType target_meter_type = ResourceToTargetMeter(m_type);

    if (INVALID_METER_TYPE == meter_type || INVALID_METER_TYPE == target_meter_type)
        ErrorLogger() << "ResourcePool::Update() called when m_type can't be converted to a valid MeterType";

    // zero to start...
    m_connected_object_groups_resource_output.clear();
    m_connected_object_groups_resource_target_output.clear();

    // temporary storage: indexed by group of systems, which objects
    // are located in that system group?
    std::map<std::set<int>, std::set<TemporaryPtr<const UniverseObject> > > system_groups_to_object_groups;


    // for every object, find if a connected system group contains the object's
    // system.  If a group does, place the object into that system group's set
    // of objects.  If no group contains the object, place the object in its own
    // single-object group.
    std::vector<TemporaryPtr<const UniverseObject> > objects = Objects().FindObjects<const UniverseObject>(m_object_ids);
    for (std::vector<TemporaryPtr<const UniverseObject> >::const_iterator it = objects.begin();
         it != objects.end(); ++it)
    {
        TemporaryPtr<const UniverseObject> obj = *it;
        int object_id = obj->ID();
        int object_system_id = obj->SystemID();
        // can't generate resources when not in a system
        if (object_system_id == INVALID_OBJECT_ID)
            continue;

        // is object's system in a system group?
        std::set<int> object_system_group;
        for (std::set<std::set<int> >::const_iterator groups_it = m_connected_system_groups.begin();
                groups_it != m_connected_system_groups.end(); ++groups_it)
        {
            const std::set<int> sys_group = *groups_it;
            if (sys_group.find(object_system_id) != sys_group.end()) {
                object_system_group = sys_group;
                break;
            }
        }

        // if object's system is not in a system group, add it as its
        // own entry in m_connected_object_groups_resource_output and m_connected_object_groups_resource_target_output
        // this will allow the object to use its own locally produced
        // resource when, for instance, distributing pp
        if (object_system_group.empty()) {
            object_system_group.insert(object_id);  // just use this already-available set to store the object id, even though it is not likely actually a system

            float obj_output = obj->GetMeter(meter_type) ? obj->CurrentMeterValue(meter_type) : 0.0f;
            m_connected_object_groups_resource_output[object_system_group] = obj_output;

            float obj_target_output = obj->GetMeter(target_meter_type) ? obj->CurrentMeterValue(target_meter_type) : 0.0f;
            m_connected_object_groups_resource_target_output[object_system_group] = obj_target_output;
            continue;
        }

        // if resource center's system is in a system group, record which system
        // group that is for later
        system_groups_to_object_groups[object_system_group].insert(obj);
    }

    // sum the resource production for object groups, and store the total
    // group production, indexed by group of object ids
    for (std::map<std::set<int>, std::set<TemporaryPtr<const UniverseObject> > >::const_iterator
         object_group_it = system_groups_to_object_groups.begin();
         object_group_it != system_groups_to_object_groups.end(); ++object_group_it)
    {
        const std::set<TemporaryPtr<const UniverseObject> >& object_group = object_group_it->second;
        std::set<int> object_group_ids;
        float total_group_output = 0.0f;
        float total_group_target_output = 0.0f;
        for (std::set<TemporaryPtr<const UniverseObject> >::const_iterator obj_it = object_group.begin();
             obj_it != object_group.end(); ++obj_it)
        {
            TemporaryPtr<const UniverseObject> obj = *obj_it;
            if (obj->GetMeter(meter_type))
                total_group_output += obj->CurrentMeterValue(meter_type);
            if (obj->GetMeter(target_meter_type))
                total_group_target_output += obj->CurrentMeterValue(target_meter_type);
            object_group_ids.insert(obj->ID());
        }
        m_connected_object_groups_resource_output[object_group_ids] = total_group_output;
        m_connected_object_groups_resource_target_output[object_group_ids] = total_group_target_output;
    }

    ChangedSignal();
}

//////////////////////////////////////////////////
// PopulationPool
//////////////////////////////////////////////////
PopulationPool::PopulationPool() :
    m_population(0.0f),
    m_growth(0.0f)
{}

float PopulationPool::Population() const
{ return m_population; }

float PopulationPool::Growth() const
{ return m_growth; }

void PopulationPool::SetPopCenters(const std::vector<int>& pop_center_ids) {
    if (m_pop_center_ids == pop_center_ids)
        return;
    m_pop_center_ids = pop_center_ids;
}

void PopulationPool::Update() {
    m_population = 0.0f;
    float future_population = 0.0f;
    // sum population from all PopCenters in this pool
    for (std::vector<int>::const_iterator it = m_pop_center_ids.begin(); it != m_pop_center_ids.end(); ++it) {
        if (TemporaryPtr<const PopCenter> center = GetPopCenter(*it)) {
            m_population += center->CurrentMeterValue(METER_POPULATION);
            future_population += center->NextTurnCurrentMeterValue(METER_POPULATION);
        }
    }
    m_growth = future_population - m_population;
    ChangedSignal();
}
