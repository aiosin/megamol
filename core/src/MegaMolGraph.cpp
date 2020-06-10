﻿#include <iostream>

#include "stdafx.h"
#include "mmcore/MegaMolGraph.h"

#include "mmcore/view/AbstractView_EventConsumption.h"

megamol::core::MegaMolGraph::MegaMolGraph(
	megamol::core::CoreInstance& core, 
    factories::ModuleDescriptionManager const& moduleProvider, factories::CallDescriptionManager const& callProvider,
		std::unique_ptr<render_api::AbstractRenderAPI> rapi,
		std::string rapi_name)
    : moduleProvider_ptr{&moduleProvider}, callProvider_ptr{&callProvider}, rapi_{std::move(rapi)}, rapi_root_name{rapi_name}, dummy_namespace{std::make_shared<RootModuleNamespace>()}
{
	// the Core Instance is a parasite that needs to be passed to all modules
	// TODO: make it so there is no more core instance
	dummy_namespace->SetCoreInstance(core);
}

/**
 * A move of the graph should be OK, even without changing state of Modules in graph.
 */
megamol::core::MegaMolGraph::MegaMolGraph(MegaMolGraph&& rhs) noexcept {}

/**
 * Same is true for move-assignment.
 */
megamol::core::MegaMolGraph& megamol::core::MegaMolGraph::operator=(MegaMolGraph&& rhs) noexcept { return *this; }

/**
 * Construction from serialized string.
 */
// megamol::core::MegaMolGraph::MegaMolGraph(std::string const& descr) {
//}

/** dtor */
megamol::core::MegaMolGraph::~MegaMolGraph() {
    if (this->rapi_)
		rapi_->closeAPI();

    moduleProvider_ptr = nullptr;
    callProvider_ptr = nullptr;
}

const megamol::core::factories::ModuleDescriptionManager& megamol::core::MegaMolGraph::ModuleProvider() {
    return *moduleProvider_ptr; // not null because we took it by reference in constructor
}

const megamol::core::factories::CallDescriptionManager& megamol::core::MegaMolGraph::CallProvider() {
    return *callProvider_ptr;
}

bool megamol::core::MegaMolGraph::QueueModuleDeletion(std::string const& id) {
    auto lock = module_deletion_queue_.AcquireLock();
    return QueueModuleDeletion(id);
}


bool megamol::core::MegaMolGraph::QueueModuleDeletionNoLock(std::string const& id) {
    module_deletion_queue_.Push(id);
    return true;
}


bool megamol::core::MegaMolGraph::QueueModuleInstantiation(std::string const& className, std::string const& id) {
    auto lock = module_instantiation_queue_.AcquireLock();
    return QueueModuleInstantiationNoLock(className, id);
}


bool megamol::core::MegaMolGraph::QueueModuleInstantiationNoLock(std::string const& className, std::string const& id) {
    module_instantiation_queue_.Emplace(ModuleInstantiationRequest{className, id});
    return true;
}


bool megamol::core::MegaMolGraph::QueueCallDeletion(std::string const& from, std::string const& to) {
    auto lock = call_deletion_queue_.AcquireLock();
    return QueueCallDeletionNoLock(from, to);
}


bool megamol::core::MegaMolGraph::QueueCallDeletionNoLock(std::string const& from, std::string const& to) {
    call_deletion_queue_.Emplace(CallDeletionRequest{from, to});
    return true;
}


bool megamol::core::MegaMolGraph::QueueCallInstantiation(
    std::string const& className, std::string const& from, std::string const& to) {
    auto lock = call_instantiation_queue_.AcquireLock();
    return QueueCallInstantiationNoLock(className, from, to);
}


bool megamol::core::MegaMolGraph::QueueCallInstantiationNoLock(
    std::string const& className, std::string const& from, std::string const& to) {
    call_instantiation_queue_.Emplace(CallInstantiationRequest{className, from, to});
    return true;
}


bool megamol::core::MegaMolGraph::HasPendingRequests() {
    auto lock = AcquireQueueLocks(); // TODO: lock necessary?
    return !module_deletion_queue_.Empty() || !module_instantiation_queue_.Empty() || !call_deletion_queue_.Empty() ||
           !call_instantiation_queue_.Empty();
}

std::scoped_lock<std::unique_lock<std::mutex>, std::unique_lock<std::mutex>, std::unique_lock<std::mutex>,
    std::unique_lock<std::mutex>>
megamol::core::MegaMolGraph::AcquireQueueLocks() {
    auto lock1 = module_deletion_queue_.AcquireDeferredLock();
    auto lock2 = module_instantiation_queue_.AcquireDeferredLock();
    auto lock3 = call_deletion_queue_.AcquireDeferredLock();
    auto lock4 = call_instantiation_queue_.AcquireDeferredLock();
    return std::scoped_lock(lock1, lock2, lock3, lock4);
}

[[nodiscard]] megamol::core::MegaMolGraph::ModuleList_t::iterator megamol::core::MegaMolGraph::find_module(
    std::string const& name) {
    std::shared_lock<std::shared_mutex> lock(graph_lock_);

    auto it = std::find_if(this->module_list_.begin(), this->module_list_.end(),
        [&name](megamol::core::MegaMolGraph::ModuleInstance_t const& el) { return el.second.id == name; });

    return it;
}

    [[nodiscard]] megamol::core::MegaMolGraph::ModuleList_t::const_iterator megamol::core::MegaMolGraph::find_module(
        std::string const& name) const {
    std::shared_lock<std::shared_mutex> lock(graph_lock_);

    auto it = std::find_if(this->module_list_.cbegin(), this->module_list_.cend(),
        [&name](megamol::core::MegaMolGraph::ModuleInstance_t const& el) { return el.second.id == name; });

    return it;
}

[[nodiscard]] megamol::core::MegaMolGraph::CallList_t::iterator megamol::core::MegaMolGraph::find_call(
    std::string const& from, std::string const& to) {
    std::shared_lock<std::shared_mutex> lock(graph_lock_);

    auto it = std::find_if(
        this->call_list_.begin(), this->call_list_.end(), [&](megamol::core::MegaMolGraph::CallInstance_t const& el) {
            return el.second.from == from && el.second.to == to;
        });

    return it;
}

    [[nodiscard]] megamol::core::MegaMolGraph::CallList_t::const_iterator megamol::core::MegaMolGraph::find_call(
        std::string const& from, std::string const& to) const {

    std::shared_lock<std::shared_mutex> lock(graph_lock_);

    auto it = std::find_if(
        this->call_list_.cbegin(), this->call_list_.cend(), [&](megamol::core::MegaMolGraph::CallInstance_t const& el) {
            return el.second.from == from && el.second.to == to;
        });

    return it;
}

// splits a string of the form "::one::two::three::" into an array of strings {"one", "two", "three"}
static std::vector<std::string> splitPathName(std::string const& path) {
    std::vector<std::string> result;

    size_t start = 0;
    while ((start = path.find_first_not_of(':', start)) != std::string::npos) {
        auto end = path.find_first_of(':', start);
        if (start < end) result.push_back(path.substr(start, end - start));
        start = end;
    }

    return result;
}


[[nodiscard]] bool megamol::core::MegaMolGraph::add_module(ModuleInstantiationRequest_t const& request) {
    factories::ModuleDescription::ptr module_description = this->ModuleProvider().Find(request.className.c_str());
    if (!module_description) return false;

    const auto path_parts = splitPathName(request.id);
    if (path_parts.empty()) return false;

    const auto module_name = vislib::StringA(path_parts.back().c_str());

    Module::ptr_type module_ptr = Module::ptr_type(module_description->CreateModule(module_name));
    if (!module_ptr) return false;

    this->module_list_.emplace_front(module_ptr, request);

	module_ptr->setParent(this->dummy_namespace);

    // execute IsAvailable() and Create() in GL context
    this->rapi_commands.emplace_front(
        [module_description, module_ptr]() { return !(module_description->IsAvailable() && module_ptr->Create()); }); // returns false if something went wrong

	// if the new module is a view module register if with a View Resource Feeder and set it up to get the default resources of the GLFW context plus an empty handler for rendering
	megamol::core::view::AbstractView* view_ptr = nullptr;
    if (view_ptr = dynamic_cast<megamol::core::view::AbstractView*>(module_ptr.get())) {
        this->view_feeders.push_back(ViewResourceFeeder{view_ptr, 
			{
				// rendering resource handlers are executed in the order defined here
				std::make_pair("KeyboardEvents", megamol::core::view::view_consume_keyboard_events),
				std::make_pair("MouseEvents", megamol::core::view::view_consume_mouse_events),
				std::make_pair("WindowEvents", megamol::core::view::view_consume_window_events),
				std::make_pair("FramebufferEvents", megamol::core::view::view_consume_framebuffer_events),
				std::make_pair("", megamol::core::view::view_poke_rendering),
			}});
    }
	
	// TODO: make sure that requested rendering resources or inputs for the view are provided by some RAPI

    return true;
}

    [[nodiscard]] bool megamol::core::MegaMolGraph::add_call(CallInstantiationRequest_t const& request) {

    factories::CallDescription::ptr call_description = this->CallProvider().Find(request.className.c_str());

    const auto getCallSlotOfModule = [this, &call_description](
                                         std::string const& name) -> std::pair<bool, AbstractSlot*> {
        const auto path = splitPathName(name);
        if (path.empty()) return {false, nullptr};

        auto module_name = path[0] + "::" + path[1];
        auto module_it = this->find_module(module_name);
        if (module_it == this->module_list_.end()) return {false, nullptr};

        Module::ptr_type module_ptr = module_it->first;
        const auto slot_name = vislib::StringA(path.back().c_str());
        AbstractSlot* slot_ptr = module_ptr->FindSlot(slot_name);
        if (!slot_ptr) return {false, nullptr};

        if (!slot_ptr->IsCallCompatible(call_description)) return {false, nullptr};

        if (!slot_ptr->GetStatus() == AbstractSlot::STATUS_ENABLED) return {false, nullptr};

        return {true, slot_ptr};
    };

    auto from_slot = getCallSlotOfModule(request.from);
    if (from_slot.first == false) return false; // error when looking for from-slot
    CallerSlot* caller = dynamic_cast<CallerSlot*>(from_slot.second);

    auto to_slot = getCallSlotOfModule(request.to);
    if (to_slot.first == false) return false; // error when looking for to-slot
    CalleeSlot* callee = dynamic_cast<CalleeSlot*>(to_slot.second);

    if ((caller->GetStatus() == AbstractSlot::STATUS_CONNECTED) ||
        (callee->GetStatus() == AbstractSlot::STATUS_CONNECTED)) {
        Call* tstCall = caller->IsConnectedTo(callee);
        if (tstCall && call_description->IsDescribing(tstCall)) {
            return false; // call already exists
        }
    }

	// TODO: kill parents of modules/calls when new graph structure is in place
	callee->setParent(this->dummy_namespace);
	caller->setParent(this->dummy_namespace);

    Call::ptr_type call = Call::ptr_type(call_description->CreateCall());
    if (!callee->ConnectCall(call.get(), call_description)) return false;
    if (!caller->ConnectCall(call.get())) {
        // FIXME: if connecting the callER fails, how to disconnect call from callEE?
        // callee->DisconnectCalls();
        return false;
    }

    this->call_list_.emplace_front(call, request);

    return true;
}

[[nodiscard]]
static
std::list<megamol::core::MegaMolGraph::CallList_t::iterator> find_all_of(megamol::core::MegaMolGraph::CallList_t list,
    std::function<bool(megamol::core::MegaMolGraph::CallInstance_t const&)> const& func) {

    std::list<megamol::core::MegaMolGraph::CallList_t::iterator> result;

    for (auto begin = list.begin(); begin != list.end(); begin++)
        if (func(*begin)) result.push_back(begin);

    return result;
}


bool megamol::core::MegaMolGraph::delete_module(ModuleDeletionRequest_t const& request) {

    auto module_it = find_module(request);
    if (module_it == this->module_list_.end()) return false;

    auto module_ptr = module_it->first; // is std::shared_ptr, a copy stays alive until rapi_commands got executed and the vector gets cleared
    if (!module_ptr) return false;

    // delete all outgoing/incoming calls
    auto discard_calls = find_all_of(call_list_,
        [&](auto const& call_info) { return (call_info.second.from == request || call_info.second.to == request); });

	std::for_each(discard_calls.begin(), discard_calls.end(), [&](auto const& call_it) {
        delete_call(CallDeletionRequest_t{call_it->second.from, call_it->second.to});
    });

    // call Release() in GL context
    this->rapi_commands.emplace_back([module_ptr]() -> bool {
        module_ptr->Release();
        return true;
		// end of lambda scope deletes last shared_ptr to module
		// thus the module gets deleted after execution and deletion of this command callback
     });

    this->module_list_.erase(module_it);

    return true;
}


bool megamol::core::MegaMolGraph::delete_call(CallDeletionRequest_t const& request) {

    auto call_it = find_call(request.from, request.to);

    if (call_it == this->call_list_.end()) return false;

    auto target = call_it->first->PeekCalleeSlotNoConst();
    auto source = call_it->first->PeekCallerSlotNoConst();

    if (!target || !source) return false;

    source->SetCleanupMark(true);
    source->DisconnectCalls();
    source->PerformCleanup();  // does nothing
    target->DisconnectCalls(); // does nothing

    this->call_list_.erase(call_it);

    return true;
}


void megamol::core::MegaMolGraph::ExecuteGraphUpdates() {
	// TODO: lock is broken?
	// TODO: is lock necessary?
    //auto lock = this->AcquireQueueLocks();

    while (!call_deletion_queue_.Empty()) {
        auto call_deletion = call_deletion_queue_.Get();
        delete_call(call_deletion);
    }

	while (!module_deletion_queue_.Empty()) {
        auto module_deletion = module_deletion_queue_.Get();
        delete_module(module_deletion);
    }

	while (!module_instantiation_queue_.Empty()) {
        auto module_request = module_instantiation_queue_.Get();
        bool creation_success = add_module(module_request);

		if (!creation_success)
			std::cout << "MegaMolGraph: module failed construction: " << module_request.id << ", " << module_request.className << std::endl;
	}

	while (!call_instantiation_queue_.Empty()) {
        auto call_request = call_instantiation_queue_.Get();
        bool creation_success = add_call(call_request);

		if (!creation_success)
			std::cout << "MegaMolGraph: call failed construction: " << call_request.className << " from " << call_request.from << " to " << call_request.to << std::endl;
	}
}

void megamol::core::MegaMolGraph::RenderNextFrame() {

    if (this->rapi_)
		this->rapi_->preViewRender();

	// OpenGL context for module Create() provided here by preViewRender() of RAPI with GL context
	bool some_command_failed = false;
	for (auto& command : this->rapi_commands)
		some_command_failed |= command(); // module Create() or Release() called here

    this->rapi_commands.clear();

	if (some_command_failed) {
		// fail and stop execution of MegaMol because without the requested graph modules further execution makes no sense
        for (auto& m : module_list_) {
            if (!m.first->isCreated())
                std::cout << "MegaMolGraph: module not created: " << m.second.id << ", " << m.second.className << std::endl;
        }
    }

	// process ui events and other resources
	// this also contains a handler that tells the view to render itself
    auto& resources = this->rapi_->getRenderResources();
    for (auto& view_feeder : view_feeders)
		view_feeder.consume(resources);

	// TODO: handle 'stop rendering' requests

    if (this->rapi_)
		this->rapi_->postViewRender();
}


//[[nodiscard]] std::shared_ptr<megamol::core::param::AbstractParam> FindParameter(std::string const& name, bool
// quiet = false) const;

// static const std::tuple<const std::string, const std::string> splitParameterName(std::string const& name) {
//     const std::string delimiter = "::";
//
//     const auto lastDeliPos = name.rfind(delimiter);
//     if (lastDeliPos == std::string::npos) return {"", ""};
//
//     auto secondLastDeliPos = name.rfind(delimiter, lastDeliPos - 1);
//     if (secondLastDeliPos == std::string::npos)
//         secondLastDeliPos = 0;
//     else
//         secondLastDeliPos = secondLastDeliPos + 2;
//
//     const std::string parameterName = name.substr(lastDeliPos + 2);
//     const auto moduleNameStartPos = secondLastDeliPos;
//     const std::string moduleName = name.substr(moduleNameStartPos, /*cont*/ lastDeliPos - moduleNameStartPos);
//
//     return {moduleName, parameterName};
// };
//
// std::shared_ptr<megamol::core::param::AbstractParam> megamol::core::MegaMolGraph::FindParameter(
//     std::string const& name, bool quiet) const {
//     using vislib::sys::Log;
//
//     // what exactly is the format of name? assumption: [::]moduleName::parameterName
//     // split name into moduleName and parameterName:
//     auto [moduleName, parameterName] = splitParameterName(name);
//
//     // parameter or module does not exist
//     if (moduleName.compare("") == 0 || parameterName.compare("") == 0) {
//         if (!quiet)
//             Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR,
//                 "Cannot find parameter \"%s\": could not extract module and parameter name", name.c_str());
//         return nullptr;
//     }
//
//     param::ParamSlot* slot = nullptr;
//     Module::ptr_type modPtr = nullptr;
//
//     for (auto& graphRoot : this->subgraphs_) {
//         for (auto& mod : graphRoot.modules_)
//             if (mod.first->Name().Compare(moduleName.c_str())) {
//                 modPtr = mod.first;
//
//                 const auto result = mod.first->FindChild((moduleName + "::" + parameterName).c_str());
//                 slot = dynamic_cast<param::ParamSlot*>(result.get());
//
//                 break;
//             }
//     }
//
//     if (!modPtr) {
//         if (!quiet)
//             Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Cannot find parameter \"%s\": module not found",
//             name.c_str());
//         return nullptr;
//     }
//
//     if (slot == nullptr) {
//         if (!quiet)
//             Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Cannot find parameter \"%s\": slot not found",
//             name.c_str());
//         return nullptr;
//     }
//     if (slot->GetStatus() == AbstractSlot::STATUS_UNAVAILABLE) {
//         if (!quiet)
//             Log::DefaultLog.WriteMsg(
//                 Log::LEVEL_ERROR, "Cannot find parameter \"%s\": slot is not available", name.c_str());
//         return nullptr;
//     }
//     if (slot->Parameter().IsNull()) {
//         if (!quiet)
//             Log::DefaultLog.WriteMsg(
//                 Log::LEVEL_ERROR, "Cannot find parameter \"%s\": slot has no parameter", name.c_str());
//         return nullptr;
//     }
//
//
//     return slot->Parameter();
// }