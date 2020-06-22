#pragma once

#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
#include "mmcore/Call.h"
#include "mmcore/Module.h"
#include "mmcore/api/MegaMolCore.h"
#include "mmcore/factories/CallDescription.h"
#include "mmcore/factories/CallDescriptionManager.h"
#include "mmcore/factories/ModuleDescription.h"
#include "mmcore/factories/ModuleDescriptionManager.h"
#include "mmcore/param/AbstractParam.h"
#include "mmcore/param/ParamSlot.h"
#include "mmcore/param/ParamUpdateListener.h"
#include "vislib/sys/Log.h"

#include "mmcore/CalleeSlot.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/deferrable_construction.h"
#include "mmcore/serializable.h"

#include "AbstractUpdateQueue.h" // TODO: why can't we use std::list? why is this class called abstract when, in fact, its implementation is very concrete?

#include "AbstractRenderAPI.hpp"
#include "mmcore/RootModuleNamespace.h"

#include "mmcore/view/AbstractView.h"
#include "RenderResource.h"

namespace megamol {
namespace core {

class MEGAMOLCORE_API MegaMolGraph : public serializable, public deferrable_construction {

    // todo: where do the descriptionmanagers go?
    // todo: what about the view / job descriptions?
public:
    // todo: constructor(s)? see serialization
    // todo: probably get rid of RootModuleNamespace altogether

    ///////////////////////////// types ////////////////////////////////////////
    using ModuleDeletionRequest_t = std::string;

    struct ModuleInstantiationRequest {
        std::string className;
        std::string id;
    };

    using ModuleInstantiationRequest_t = ModuleInstantiationRequest;

    struct CallDeletionRequest {
        std::string from;
        std::string to;
    };

    using CallDeletionRequest_t = CallDeletionRequest;

    struct CallInstantiationRequest {
        std::string className;
        std::string from;
        std::string to;
    };

    using CallInstantiationRequest_t = CallInstantiationRequest;

    using ModuleInstance_t = std::pair<Module::ptr_type, ModuleInstantiationRequest>;

    using ModuleList_t = std::list<ModuleInstance_t>;

    using CallInstance_t = std::pair<Call::ptr_type, CallInstantiationRequest>;

    using CallList_t = std::list<CallInstance_t>;

    using lua_task_result_t = std::tuple<int, std::string>;
    using lua_task_future_t = std::future<lua_task_result_t>;

    struct lua_task {
        std::string script;
        std::optional<lua_task_future_t> future;
    };

    using lua_tasks_queue_t = std::list<lua_task>;

    //////////////////////////// ctor / dtor ///////////////////////////////

    /**
     * Bare construction as stub for deserialization
     */
    MegaMolGraph(megamol::core::CoreInstance& core, factories::ModuleDescriptionManager const& moduleProvider,
        factories::CallDescriptionManager const& callProvider,
		std::unique_ptr<render_api::AbstractRenderAPI> rapi,
		std::string rapi_name);

    /**
     * No copy-construction. This can only be a legal operation, if we allow deep-copy of Modules in graph.
     */
    MegaMolGraph(MegaMolGraph const& rhs) = delete;

    /**
     * Same argument as for copy-construction.
     */
    MegaMolGraph& operator=(MegaMolGraph const& rhs) = delete;

    /**
     * A move of the graph should be OK, even without changing state of Modules in graph.
     */
    MegaMolGraph(MegaMolGraph&& rhs) noexcept;

    /**
     * Same is true for move-assignment.
     */
    MegaMolGraph& operator=(MegaMolGraph&& rhs) noexcept;

    /**
     * Construction from serialized string.
     */
    // MegaMolGraph(std::string const& descr);

    /** dtor */
    virtual ~MegaMolGraph();

    //////////////////////////// END ctor / dtor ///////////////////////////////


    //////////////////////////// Satisfy some abstract requirements ///////////////////////////////


	// TODO: the 'serializable' and 'deferrable construction' concepts result in basically the same implementation?
	// serializable
    std::string Serialize() const override { return ""; };
    void Deserialize(std::string const& descr) override{};

	// deferrable_construction 
    bool create() override { return false; };
    void release() override{};

    //////////////////////////// Modules and Calls loaded from DLLs ///////////////////////////////

private:
    const factories::ModuleDescriptionManager& ModuleProvider();
    const factories::CallDescriptionManager& CallProvider();
    const factories::ModuleDescriptionManager* moduleProvider_ptr;
    const factories::CallDescriptionManager* callProvider_ptr;

public:
    /*
     * Each module should be serializable, i.e. the modules capture their entire state.
     * As a result, an entire MegaMolGraph can basically be copied by reinitializing the serialized descriptor.
     * Therefore the MegaMolGraph creates its descriptor by iterating through all modules and calls in the graph.
     *
     * Maybe the ModuleGraph should even allow external objects to iterate through a linearized array of containing
     * modules and calls.
     */

    bool DeleteModule(std::string const& id);

    bool CreateModule(std::string const& className, std::string const& id);

    bool DeleteCall(std::string const& from, std::string const& to);

    bool CreateCall(std::string const& className, std::string const& from, std::string const& to);

    megamol::core::Module::ptr_type FindModule(std::string const& moduleName) const;

    megamol::core::Call::ptr_type FindCall(std::string const& from, std::string const& to) const;

    megamol::core::param::AbstractParam* FindParameter(std::string const& paramName) const;

    std::vector<megamol::core::param::AbstractParam*> FindModuleParameters(std::string const& moduleName) const;

    CallList_t const& ListCalls() const;

    ModuleList_t const& ListModules() const;

    std::vector<megamol::core::param::AbstractParam*> ListParameters() const;

    std::optional<lua_task_future_t> QueueLuaScript(std::string const& script, bool want_result);

    bool HasPendingRequests();

    void RenderNextFrame();

	// Create View ?

	// Create Chain Call ?

    //int ListInstatiations(lua_State* L);

private:
    // get invalidated and the user is helpless
    [[nodiscard]] ModuleList_t::iterator find_module(std::string const& name);

    [[nodiscard]] ModuleList_t::const_iterator find_module(std::string const& name) const;

    [[nodiscard]] CallList_t::iterator find_call(std::string const& from, std::string const& to);

    [[nodiscard]] CallList_t::const_iterator find_call(std::string const& from, std::string const& to) const;

    [[nodiscard]] bool add_module(ModuleInstantiationRequest_t const& request);

    [[nodiscard]] bool add_call(CallInstantiationRequest_t const& request);

    bool delete_module(ModuleDeletionRequest_t const& request);

    bool delete_call(CallDeletionRequest_t const& request);


    /** List of modules that this graph owns */
    ModuleList_t module_list_;

    // the dummy_namespace must be above the call_list_ because it needs to be destroyed AFTER all calls during
    // ~MegaMolGraph()
    std::shared_ptr<RootModuleNamespace> dummy_namespace; // serves as parent object for stupid fat modules

    /** List of call that this graph owns */
    CallList_t call_list_;

    // Render APIs (RAPIs) provide rendering or other resources (e.g. OpenGL context, user inputs)
    // that are used by MegaMol for rendering. We may use many RAPIs simultaneously that provide data to MegaMol,
    // but we need at least one RAPI that provides data like rendering context and rendering configuration (i.e. framebuffer memory and size).
    // Views (e.g. View3D) are the entry point from where the MegaMol graph is evaluated. 
    // Views are fed with resources and events that are provided by RAPIs.
    // things get compilcated when we try to model having many different Views that depend on many different resources coming from different RAPIs.
    // because of this, for now we only have one RAPI object in the feeding events to Views and we expect only one View object to be present in the graph.
    std::unique_ptr<render_api::AbstractRenderAPI> rapi_;
    std::string rapi_root_name;
    std::list<std::function<bool()>> rapi_commands;

	struct RapiAutoExecution {
        render_api::AbstractRenderAPI* ptr = nullptr;

        RapiAutoExecution(std::unique_ptr<render_api::AbstractRenderAPI>& rapi);
        ~RapiAutoExecution();
	};

	// this struct feeds a view instance with requested render resources like keyboard events or framebuffer resizes
	// resources are identified by name (a string). when a requested resource name is found 
	// the resource is passed to a handler function that is registered with the ViewResourceFeeder
	// the handler function is supposed to pass the resource to the AbstractView object in some way (e.g. pass keyboard inputs to OnKey callbacks of the View)
	// one may register 'empty' handlers that dont expect any resource but should be executed anyway. those ResourceHandlers use the empty string "" as resource identification.
	// currently empty handlers are used to tell a View to render itself after all input events have been processed.
    struct ViewResourceFeeder {
        view::AbstractView* view;

		using ResourceHandler =
            std::pair<std::string, std::function<void(view::AbstractView&, const megamol::render_api::RenderResource&)>>;

        std::vector<ResourceHandler> resourceConsumption;

        void consume(const std::vector<megamol::render_api::RenderResource>& resources) {
            if (!view)
				return; // TODO: big fail

            std::for_each(resourceConsumption.begin(), resourceConsumption.end(), 
				[&](const ResourceHandler& resource_handler) {
					auto resource_it = std::find_if(resources.begin(), resources.end(), 
						[&](const megamol::render_api::RenderResource& resource) { return resource_handler.first == resource.getIdentifier() || resource_handler.first == "" || resource_handler.first.empty(); });

					if (resource_it != resources.end() && resource_it->getIdentifier() == resource_handler.first) {
                        resource_handler.second(*view, *resource_it);
                    } 
					if(resource_handler.first == "" || resource_handler.first.empty()) {
                        resource_handler.second(*view, megamol::render_api::RenderResource{"", std::nullopt});
                    }
				});
		}
    };

	// for each View in the MegaMol graph we create a ViewResourceFeeder with corresponding ResourceHandlers for resource/input consumption
	// currently the setup of the ResourceHandlers happens when the graph recognizes that a newly created module is a view.
	// right now this happens in MegaMolGraph::add_module(), but it may be automated or configured from outside, 
	// for example by passing resource handler names that should be passed to the View when requesting a View module in the config file
    std::list<ViewResourceFeeder> view_feeders;


    ////////////////////////// old interface stuff //////////////////////////////////////////////
public:
    // TODO: pull necessary 'old' functions to active section above
    /*
        bool QueueChainCallInstantiation(const std::string className, const std::string chainStart, const
    std::string to); bool QueueParamValueChange(const std::string id, const std::string value);
        // a JSON serialization of all the requests as above (see updateListener)
        bool QueueGraphChange(const std::string changes);
        /// @return group id 0 is invalid and means failure
        uint32_t CreateParamGroup(const std::string name, int size);
        bool QueueParamGroupValue(const std::string groupName, const std::string id, const std::string value);
        bool QueueParamGroupValue(uint32_t groupId, const std::string id, const std::string value);
        bool QueueUpdateFlush();
        bool AnythingQueued();

        // todo: for everything below, RO version AND RW version? or would we just omit the const and imply the user
    needs
        // to lock?
        ////////////////////////////

        // vislib::SmartPtr<param::AbstractParam> FindParameter(const std::string name, bool quiet = false) const;

        // todo: optionally ask for the parameters of a specific module (name OR module pointer?)
        inline void EnumerateParameters(std::function<void(const Module&, param::ParamSlot&)> cb) const;

        // todo: optionally pass Module instead of name
        template <class A, class C>
        typename std::enable_if<std::is_convertible<A*, Module*>::value && std::is_convertible<C*, Call*>::value,
            bool>::type
        EnumerateCallerSlots(std::string module_name, std::function<void(C&)> cb) const;

        // WHY??? this is just EnumerateParameters(FindModule()...) GET RID OF IT!
        template <class A>
        typename std::enable_if<std::is_convertible<A*, Module*>::value, bool>::type EnumerateParameterSlots(
            std::string module_name, std::function<void(param::ParamSlot&)> cb) const;

        size_t GetGlobalParameterHash(void) const;

        // probably just throws everything away?
        void Cleanup(void);

        // serialize into... JSON? WHY THE FUCK IS THIS IN THE ROOTMODULENAMESPACE!
        std::string SerializeGraph(void) const;

        // replace the whole graph with whatever is in serialization
        void Deserialize(std::string othergraph);

        // nope, see below!
        // void NotifyParameterValueChange(param::ParamSlot& slot) const;
        // void RegisterParamUpdateListener(param::ParamUpdateListener* pul);
        // void UnregisterParamUpdateListener(param::ParamUpdateListener* pul);

        // accumulate the stuff the queues ask from the graph and give out a JSON diff right afterwards
        // bitfield says what (params, modules, whatnot) - and also reports whether something did not happen
        /// @return some ID to allow for removal of the listener later
        uint32_t RegisterGraphUpdateListener(
            std::function<void(std::string, uint32_t field)> func, int32_t serviceBitfield);
        void UnregisterGraphUpdateListener(uint32_t id);

    private:
        // todo: signature is weird, data structure might be as well
        void computeGlobalParameterHash(ModuleNamespace::const_ptr_type path, ParamHashMap_t& map) const;
        static void compareParameterHash(ParamHashMap_t& one, ParamHashMap_t& other) const;

        void updateFlushIdxList(size_t const processedCount, std::vector<size_t>& list);
        bool checkForFlushEvent(size_t const eventIdx, std::vector<size_t>& list) const;
        void shortenFlushIdxList(size_t const eventCount, std::vector<size_t>& list);
    */

    //    [[nodiscard]] std::shared_ptr<param::AbstractParam> FindParameter(
    //        std::string const& name, bool quiet = false) const;
};


} /* namespace core */
} // namespace megamol

