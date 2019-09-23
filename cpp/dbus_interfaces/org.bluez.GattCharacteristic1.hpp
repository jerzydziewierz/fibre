#ifndef __INTERFACES__ORG_BLUEZ_GATTCHARACTERISTIC1_HPP
#define __INTERFACES__ORG_BLUEZ_GATTCHARACTERISTIC1_HPP

#include <fibre/dbus.hpp>
#include <fibre/closure.hpp>
#include <vector>

class org_bluez_GattCharacteristic1 {
public:
    struct tag {};
    static const char* get_interface_name() { return "org.bluez.GattCharacteristic1"; }

    org_bluez_GattCharacteristic1(fibre::DBusRemoteObjectBase* base)
        : base_(base) {}
    
    // For now we delete the copy constructor as we would need to change the references within the signal objects for copying an object properly
    org_bluez_GattCharacteristic1(const org_bluez_GattCharacteristic1 &) = delete;
    org_bluez_GattCharacteristic1& operator=(const org_bluez_GattCharacteristic1 &) = delete;


    int ReadValue_async(fibre::Callback<org_bluez_GattCharacteristic1*, std::vector<uint8_t>>* callback) {
        return base_->method_call_async(this, "ReadValue", callback);
    }

    int WriteValue_async(std::vector<uint8_t> value, fibre::Callback<org_bluez_GattCharacteristic1*>* callback) {
        return base_->method_call_async(this, "WriteValue", callback, value);
    }

    int StartNotify_async(fibre::Callback<org_bluez_GattCharacteristic1*>* callback) {
        return base_->method_call_async(this, "StartNotify", callback);
    }

    int StopNotify_async(fibre::Callback<org_bluez_GattCharacteristic1*>* callback) {
        return base_->method_call_async(this, "StopNotify", callback);
    }

    // DBusProperty<std::string> UUID;
    // DBusProperty<fibre::DBusObjectPath> Service;
    // DBusProperty<std::vector<uint8_t>> Value;
    // DBusProperty<bool> Notifying;
    // DBusProperty<std::vector<std::string>> Flags;
    // DBusProperty<std::vector<fibre::DBusObjectPath>> Descriptors;

    struct ExportTable : fibre::ExportTableBase {
        ExportTable() : fibre::ExportTableBase{
            { "ReadValue", fibre::FunctionImplTable{} },
            { "WriteValue", fibre::FunctionImplTable{} },
            { "StartNotify", fibre::FunctionImplTable{} },
            { "StopNotify", fibre::FunctionImplTable{} },
        } {}
        std::unordered_map<fibre::dbus_type_id_t, size_t> ref_count{}; // keeps track of how often a given type has been registered

        template<typename ... TArgs>
        using signal_closure_t = fibre::Closure<void(fibre::DBusConnectionWrapper::*)(std::string, fibre::DBusObjectPath, TArgs...), std::tuple<fibre::DBusConnectionWrapper*, std::string, fibre::DBusObjectPath>, std::tuple<TArgs...>, void>;

        template<typename ... TArgs>
        using signal_table_entry_t = std::pair<signal_closure_t<TArgs...>, void(*)(void*, signal_closure_t<TArgs...>&)>;

        template<typename TImpl>
        void register_implementation(fibre::DBusConnectionWrapper& conn, fibre::DBusObjectPath path, TImpl& obj) {
            if (ref_count[fibre::get_type_id<TImpl>()]++ == 0) {
                (*this)["ReadValue"].insert({fibre::get_type_id<TImpl>(), [](void* obj, DBusMessage* rx_msg, DBusMessage* tx_msg){ return fibre::DBusConnectionWrapper::handle_method_call_typed(rx_msg, tx_msg, fibre::make_tuple_closure(&TImpl::ReadValue, (TImpl*)obj, (std::tuple<std::vector<uint8_t>>*)nullptr)); }});
                (*this)["WriteValue"].insert({fibre::get_type_id<TImpl>(), [](void* obj, DBusMessage* rx_msg, DBusMessage* tx_msg){ return fibre::DBusConnectionWrapper::handle_method_call_typed(rx_msg, tx_msg, fibre::make_tuple_closure(&TImpl::WriteValue, (TImpl*)obj, (std::tuple<>*)nullptr)); }});
                (*this)["StartNotify"].insert({fibre::get_type_id<TImpl>(), [](void* obj, DBusMessage* rx_msg, DBusMessage* tx_msg){ return fibre::DBusConnectionWrapper::handle_method_call_typed(rx_msg, tx_msg, fibre::make_tuple_closure(&TImpl::StartNotify, (TImpl*)obj, (std::tuple<>*)nullptr)); }});
                (*this)["StopNotify"].insert({fibre::get_type_id<TImpl>(), [](void* obj, DBusMessage* rx_msg, DBusMessage* tx_msg){ return fibre::DBusConnectionWrapper::handle_method_call_typed(rx_msg, tx_msg, fibre::make_tuple_closure(&TImpl::StopNotify, (TImpl*)obj, (std::tuple<>*)nullptr)); }});
            }
        }

        int deregister_implementation(fibre::DBusConnectionWrapper& conn, fibre::DBusObjectPath path, void* obj, fibre::dbus_type_id_t type_id) {
            auto it = ref_count.find(type_id);
            if (it == ref_count.end()) {
                return -1;
            }
            if (--(it->second) == 0) {
                (*this)["ReadValue"].erase((*this)["ReadValue"].find(type_id));
                (*this)["WriteValue"].erase((*this)["WriteValue"].find(type_id));
                (*this)["StartNotify"].erase((*this)["StartNotify"].find(type_id));
                (*this)["StopNotify"].erase((*this)["StopNotify"].find(type_id));
                ref_count.erase(it);
            }
            return 0;
        }
    };

    fibre::DBusRemoteObjectBase* base_;
};

#endif // __INTERFACES__ORG_BLUEZ_GATTCHARACTERISTIC1_HPP