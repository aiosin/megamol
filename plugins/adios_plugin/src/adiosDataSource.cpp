#include "stdafx.h"
#include "adiosDataSource.h"
#include "mmcore/cluster/mpi/MpiCall.h"
#include "mmcore/param/FilePathParam.h"
#include "vislib/Trace.h"
#include "vislib/sys/CmdLineProvider.h"
#include "vislib/sys/Log.h"
#include "vislib/sys/SystemInformation.h"
#include <algorithm>

namespace megamol {
namespace adios {

adiosDataSource::adiosDataSource(void)
    : core::Module()
    , filename("filename", "The path to the ADIOS-based file to load.")
    , getData("getdata", "Slot to request data from this data source.")
    , callRequestMpi("requestMpi", "Requests initialisation of MPI and the communicator for the view.")
    , data_hash(0)
    , io(nullptr)
    , reader()
    , variables()
    , dataMap()
    , loadedFrameID(0) {

    this->filename.SetParameter(new core::param::FilePathParam(""));
    this->filename.SetUpdateCallback(&adiosDataSource::filenameChanged);
    this->MakeSlotAvailable(&this->filename);


    this->getData.SetCallback("CallADIOSData", "GetData", &adiosDataSource::getDataCallback);
    this->getData.SetCallback("CallADIOSData", "GetHeader", &adiosDataSource::getHeaderCallback);
    this->MakeSlotAvailable(&this->getData);

    this->callRequestMpi.SetCompatibleCall<core::cluster::mpi::MpiCallDescription>();
    this->MakeSlotAvailable(&this->callRequestMpi);
}

adiosDataSource::~adiosDataSource(void) { this->Release(); }

/*
 * adiosDataSource::create
 */
bool adiosDataSource::create(void) {
    MpiInitialized = this->initMPI();
    vislib::sys::Log::DefaultLog.WriteInfo("ADIOS2: Initializing");
    if (MpiInitialized) {
        adiosInst = adios2::ADIOS(this->mpi_comm_, adios2::DebugON);
    } else {
        adiosInst = adios2::ADIOS(adios2::DebugON);
    }

    vislib::sys::Log::DefaultLog.WriteInfo("ADIOS2: Declaring IO");
    io = std::make_shared<adios2::IO>(adiosInst.DeclareIO("ReadBP"));

    return true;
}


/*
 * adiosDataSource::release
 */
void adiosDataSource::release(void) { /* empty */
}


/*
 * adiosDataSource::getDataCallback
 */
bool adiosDataSource::getDataCallback(core::Call& caller) {
    CallADIOSData* cad = dynamic_cast<CallADIOSData*>(&caller);
    if (cad == NULL) return false;

    if (this->data_hash != cad->getDataHash() || loadedFrameID != cad->getFrameIDtoLoad()) {

        try {

            vislib::sys::Log::DefaultLog.WriteInfo(
                "ADIOS2datasource: Stepping to frame number: %d", cad->getFrameIDtoLoad());
            if (cad->getFrameIDtoLoad() != 0) {
                for (size_t i = 0; i < cad->getFrameIDtoLoad() -1; i++) {
                    reader.BeginStep();
                    reader.EndStep();
                }
            }

            vislib::sys::Log::DefaultLog.WriteInfo("ADIOS2: Beginning step");
            adios2::StepStatus status = reader.BeginStep();
            if (status != adios2::StepStatus::OK) {
                vislib::sys::Log::DefaultLog.WriteError("ADIOS2 ERROR: BeginStep returned an error.");
                return false;
            }


            auto varsToInquire = cad->getVarsToInquire();
            if (varsToInquire.empty()) {
                vislib::sys::Log::DefaultLog.WriteError("adiosDataSource: varsToInquire is empty.");
                return false;
            }

            std::vector<std::size_t> timesteps;

            for (auto toInq : varsToInquire) {
                for (auto var : variables) {
                    if (var.first == toInq) {
                        size_t num = std::stoi(var.second["Shape"]);
                        timesteps.push_back(std::stoi(var.second["AvailableStepsCount"]));
                        if (var.second["Type"] == "float") {

                            auto fc = std::make_shared<FloatContainer>(FloatContainer());
                            //fc->dataVec.resize(num);
                            std::vector<float>& tmp_vec = fc->getVec();
                            tmp_vec.resize(num);

                            adios2::Variable<float> advar = io->InquireVariable<float>(var.first);
                            if (this->MpiInitialized) {
                                advar.SetSelection({{num * this->mpiRank}, {num}});
                            }

                            reader.Get<float>(advar, tmp_vec);
                            //reader.Get<float>(adflt, fc->dataVec);

                            dataMap[var.first] = std::move(fc);

                        } else if (var.second["Type"] == "double") {

                            auto fc = std::make_shared<DoubleContainer>(DoubleContainer());
                            std::vector<double>& tmp_vec = fc->getVec();
                            tmp_vec.resize(num);

                            adios2::Variable<double> advar = io->InquireVariable<double>(var.first);
                            if (this->MpiInitialized) {
                                advar.SetSelection({{num * this->mpiRank}, {num}});
                            }

                            reader.Get<double>(advar, tmp_vec);

                            dataMap[var.first] = std::move(fc);

                        } else if (var.second["Type"] == "int") {

                            auto fc = std::make_shared<IntContainer>(IntContainer());
                            std::vector<int>& tmp_vec = fc->getVec();
                            tmp_vec.resize(num);

                            adios2::Variable<int> advar = io->InquireVariable<int>(var.first);
                            if (this->MpiInitialized) {
                                advar.SetSelection({{num * this->mpiRank}, {num}});
                            }

                            reader.Get<int>(advar, tmp_vec);

                            dataMap[var.first] = std::move(fc);
                        }
                    }
                }
            }

            // Check of all variables have same timestep count
            std::sort(timesteps.begin(), timesteps.end()); 
            auto last = std::unique(timesteps.begin(), timesteps.end());
            timesteps.erase(last, timesteps.end()); 

            if (timesteps.size() != 1) {
                vislib::sys::Log::DefaultLog.WriteWarn(
                    "Detected variables with different count of time steps - Using lowest");
                cad->setFrameCount(*std::min_element(timesteps.begin(), timesteps.end()));
            } else {
                cad->setFrameCount(timesteps[0]);
             }

            reader.EndStep();

            // reader.Close();
            // here data is loaded
        } catch (std::invalid_argument& e) {
            vislib::sys::Log::DefaultLog.WriteError(
                "Invalid argument exception, STOPPING PROGRAM from rank %d", this->mpiRank);
            vislib::sys::Log::DefaultLog.WriteError(e.what());
        } catch (std::ios_base::failure& e) {
            vislib::sys::Log::DefaultLog.WriteError(
                "IO System base failure exception, STOPPING PROGRAM from rank %d", this->mpiRank);
            vislib::sys::Log::DefaultLog.WriteError(e.what());
        } catch (std::exception& e) {
            vislib::sys::Log::DefaultLog.WriteError("Exception, STOPPING PROGRAM from rank %d", this->mpiRank);
            vislib::sys::Log::DefaultLog.WriteError(e.what());
        }

        cad->setData(std::make_shared<adiosDataMap>(dataMap));
        cad->setDataHash(this->data_hash);
    }
    return true;
}


/*
 * adiosDataSource::filenameChanged
 */
bool adiosDataSource::filenameChanged(core::param::ParamSlot& slot) {
    using vislib::sys::Log;
    this->data_hash++;

    this->first_step = true;
    this->frameCount = 1;

    return true;
}


bool adiosDataSource::getHeaderCallback(core::Call& caller) {
    CallADIOSData* cad = dynamic_cast<CallADIOSData*>(&caller);
    if (cad == nullptr) return false;

    if (this->data_hash != cad->getDataHash() || loadedFrameID != cad->getFrameIDtoLoad()) {

        try {
            vislib::sys::Log::DefaultLog.WriteInfo("ADIOS2: Setting Engine");
            // io.SetEngine("InSituMPI");
            io->SetEngine("bpfile");
            io->SetParameter("verbose", "5");
            const std::string fname = std::string(T2A(this->filename.Param<core::param::FilePathParam>()->Value()));

            vislib::sys::Log::DefaultLog.WriteInfo("ADIOS2: Opening File %s", fname.c_str());

            this->reader = io->Open(fname, adios2::Mode::Read);

            // vislib::sys::Log::DefaultLog.WriteInfo("ADIOS2: Reading available attributes");
            // auto availAttrib = io->AvailableAttributes();
            // vislib::sys::Log::DefaultLog.WriteInfo("ADIOS2: Number of attributes %d", availAttrib.size());

            this->variables = io->AvailableVariables();
            vislib::sys::Log::DefaultLog.WriteInfo("ADIOS2: Number of variables %d", variables.size());

            std::vector<std::string> availVars;
            availVars.reserve(variables.size());

            for (auto var : variables) {
                availVars.push_back(var.first);
                vislib::sys::Log::DefaultLog.WriteInfo("%s", var.first.c_str());
            }

            cad->setAvailableVars(availVars);

        } catch (std::invalid_argument& e) {
            vislib::sys::Log::DefaultLog.WriteError(
                "Invalid argument exception, STOPPING PROGRAM from rank %d", this->mpiRank);
            vislib::sys::Log::DefaultLog.WriteError(e.what());
        } catch (std::ios_base::failure& e) {
            vislib::sys::Log::DefaultLog.WriteError(
                "IO System base failure exception, STOPPING PROGRAM from rank %d", this->mpiRank);
            vislib::sys::Log::DefaultLog.WriteError(e.what());
        } catch (std::exception& e) {
            vislib::sys::Log::DefaultLog.WriteError("Exception, STOPPING PROGRAM from rank %d", this->mpiRank);
            vislib::sys::Log::DefaultLog.WriteError(e.what());
        }
    }
    return true;
}

bool adiosDataSource::initMPI() {
    bool retval = false;
#ifdef WITH_MPI
    if (this->mpi_comm_ == MPI_COMM_NULL) {
        VLTRACE(vislib::Trace::LEVEL_INFO, "adiosDataSource: Need to initialize MPI\n");
        auto c = this->callRequestMpi.CallAs<core::cluster::mpi::MpiCall>();
        if (c != nullptr) {
            /* New method: let MpiProvider do all the stuff. */
            if ((*c)(core::cluster::mpi::MpiCall::IDX_PROVIDE_MPI)) {
                vislib::sys::Log::DefaultLog.WriteInfo("Got MPI communicator.");
                this->mpi_comm_ = c->GetComm();
            } else {
                vislib::sys::Log::DefaultLog.WriteError(_T("Could not ")
                                                        _T("retrieve MPI communicator for the MPI-based view ")
                                                        _T("from the registered provider module."));
            }
        } else {
            int initializedBefore = 0;
            MPI_Initialized(&initializedBefore);
            if (!initializedBefore) {
                this->mpi_comm_ = MPI_COMM_WORLD;
                vislib::sys::CmdLineProviderA cmdLine(::GetCommandLineA());
                int argc = cmdLine.ArgC();
                char** argv = cmdLine.ArgV();
                ::MPI_Init(&argc, &argv);
            }
        }

        if (this->mpi_comm_ != MPI_COMM_NULL) {
            vislib::sys::Log::DefaultLog.WriteInfo(_T("MPI is ready, ")
                                                   _T("retrieving communicator properties ..."));
            ::MPI_Comm_rank(this->mpi_comm_, &this->mpiRank);
            ::MPI_Comm_size(this->mpi_comm_, &this->mpiSize);
            vislib::sys::Log::DefaultLog.WriteInfo(_T("This view on %hs is %d ")
                                                   _T("of %d."),
                vislib::sys::SystemInformation::ComputerNameA().PeekBuffer(), this->mpiRank, this->mpiSize);
        } /* end if (this->comm != MPI_COMM_NULL) */
        VLTRACE(vislib::Trace::LEVEL_INFO, "adiosDataSource: MPI initialized: %s (%i)\n",
            this->mpi_comm_ != MPI_COMM_NULL ? "true" : "false", mpi_comm_);
    } /* end if (this->comm == MPI_COMM_NULL) */

    /* Determine success of the whole operation. */
    retval = (this->mpi_comm_ != MPI_COMM_NULL);
#endif /* WITH_MPI */
    return retval;
}


} // namespace adios
} // namespace megamol
