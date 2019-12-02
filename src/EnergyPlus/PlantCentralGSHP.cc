// EnergyPlus, Copyright (c) 1996-2019, The Board of Trustees of the University of Illinois,
// The Regents of the University of California, through Lawrence Berkeley National Laboratory
// (subject to receipt of any required approvals from the U.S. Dept. of Energy), Oak Ridge
// National Laboratory, managed by UT-Battelle, Alliance for Sustainable Energy, LLC, and other
// contributors. All rights reserved.
//
// NOTICE: This Software was developed under funding from the U.S. Department of Energy and the
// U.S. Government consequently retains certain rights. As such, the U.S. Government has been
// granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable,
// worldwide license in the Software to reproduce, distribute copies to the public, prepare
// derivative works, and perform publicly and display publicly, and to permit others to do so.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National Laboratory,
//     the University of Illinois, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without specific prior
//     written permission.
//
// (4) Use of EnergyPlus(TM) Name. If Licensee (i) distributes the software in stand-alone form
//     without changes from the version obtained under this License, or (ii) Licensee makes a
//     reference solely to the software portion of its product, Licensee must refer to the
//     software as "EnergyPlus version X" software, where "X" is the version number Licensee
//     obtained under this License and may not use a different name for the software. Except as
//     specifically required in this Section (4), Licensee shall not use in a company name, a
//     product name, in advertising, publicity, or other promotional activities any name, trade
//     name, trademark, logo, or other designation of "EnergyPlus", "E+", "e+" or confusingly
//     similar designation, without the U.S. Department of Energy's prior written consent.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// C++ Headers
#include <cmath>
#include <string>

// ObjexxFCL Headers
#include <ObjexxFCL/Array.functions.hh>
#include <ObjexxFCL/Fmath.hh>
#include <ObjexxFCL/gio.hh>

// EnergyPlus Headers
#include <EnergyPlus/BranchNodeConnections.hh>
#include <EnergyPlus/CurveManager.hh>
#include <EnergyPlus/DataBranchAirLoopPlant.hh>
#include <EnergyPlus/DataEnvironment.hh>
#include <EnergyPlus/DataHVACGlobals.hh>
#include <EnergyPlus/DataIPShortCuts.hh>
#include <EnergyPlus/DataLoopNode.hh>
#include <EnergyPlus/DataPlant.hh>
#include <EnergyPlus/DataSizing.hh>
#include <EnergyPlus/EMSManager.hh>
#include <EnergyPlus/FluidProperties.hh>
#include <EnergyPlus/General.hh>
#include <EnergyPlus/InputProcessing/InputProcessor.hh>
#include <EnergyPlus/NodeInputManager.hh>
#include <EnergyPlus/OutputProcessor.hh>
#include <EnergyPlus/OutputReportPredefined.hh>
#include <EnergyPlus/PlantCentralGSHP.hh>
#include <EnergyPlus/PlantUtilities.hh>
#include <EnergyPlus/ReportSizingManager.hh>
#include <EnergyPlus/ScheduleManager.hh>
#include <EnergyPlus/UtilityRoutines.hh>

namespace EnergyPlus {

// Contents:
// CentralHeatPumpSystem (CGSHP) System
// ChillerHeaterPerformance:Electric:EIR

namespace PlantCentralGSHP {

    // MODULE INFORMATION:
    //       AUTHOR         PNNL
    //       DATE WRITTEN   Feb 2013
    //       MODIFIED       na
    //       RE-ENGINEERED  na
    // PURPOSE OF THIS MODULE:
    // This module simulates the performance of the Central Plant GSHP systems
    // It currently includes one object: ChillerHeaterPerformance:Electric:EIR.
    // The other object available for this central CGSHP system such as HeatPumpPerformance:WaterToWater:EIR
    //      will be implemented later.

    // METHODOLOGY EMPLOYED:
    //  Once the PlantLoopManager determines that the Central Plant GSHP
    //  is available to meet a loop cooling and heating demands, it calls SimCentralGroundSourceHeatPump
    //  which in turn calls the electric PlantCentralGSHP model. The PlantCentralGSHP model is based on
    //  polynomial fits of chiller/heater or heat pump performance data.

    int const WaterCooled(2);
    int const SmartMixing(1);

    bool getInputWrapper(true);       // When TRUE, calls subroutine to read input file.

    int numWrappers(0);               // Number of Wrappers specified in input
    int numChillerHeaters(0);         // Number of Chiller/heaters specified in input

    Real64 ChillerCapFT(0.0);         // Chiller/heater capacity fraction (evaluated as a function of temperature)
    Real64 ChillerEIRFT(0.0);         // Chiller/heater electric input ratio (EIR = 1 / COP) as a function of temperature
    Real64 ChillerEIRFPLR(0.0);       // Chiller/heater EIR as a function of part-load ratio (PLR)
    Real64 ChillerPartLoadRatio(0.0); // Chiller/heater part-load ratio (PLR)
    Real64 ChillerCyclingRatio(0.0);  // Chiller/heater cycling ratio
    Real64 ChillerFalseLoadRate(0.0); // Chiller/heater false load over and above the water-side load [W]

    Array1D_bool CheckEquipName;

    Array1D<WrapperSpecs> Wrapper;
    Array1D<WrapperReportVars> WrapperReport;
    Array1D<ChillerHeaterSpecs> ChillerHeater;
    Array1D<CHReportVars> ChillerHeaterReport;

    void clear_state()
    {
        getInputWrapper = true;
        numWrappers = 0;
        numChillerHeaters = 0;

        ChillerCapFT = 0.0;
        ChillerEIRFT = 0.0;
        ChillerEIRFPLR = 0.0;
        ChillerPartLoadRatio = 0.0;
        ChillerCyclingRatio = 0.0;
        ChillerFalseLoadRate = 0.0;

        Wrapper.deallocate();
        WrapperReport.deallocate();
        ChillerHeater.deallocate();
        ChillerHeaterReport.deallocate();

    }

    void SimCentralGroundSourceHeatPump(std::string const &WrapperName, // User specified name of wrapper
                                        int const EquipFlowCtrl,        // Flow control mode for the equipment
                                        int &CompIndex,                 // Chiller number pointer
                                        int const LoopNum,              // plant loop index pointer
                                        bool const RunFlag,             // Simulate chiller when TRUE
                                        bool const FirstIteration,      // Initialize variables when TRUE
                                        bool &InitLoopEquip,            // If not zero, calculate the max load for operating conditions
                                        Real64 &MyLoad,                 // Loop demand component will meet [W]
                                        Real64 &MaxCap,                 // Maximum operating capacity of chiller [W]
                                        Real64 &MinCap,                 // Minimum operating capacity of chiller [W]
                                        Real64 &OptCap,                 // Optimal operating capacity of chiller [W]
                                        bool const GetSizingFactor,     // TRUE when just the sizing factor is requested
                                        Real64 &SizingFactor            // sizing factor
    )
    {
        int WrapperNum;        // Wrapper number pointer

        // Get user input values
        if (getInputWrapper) {
            GetWrapperInput();
            getInputWrapper = false;
        }

        // Find the correct wrapper
        if (CompIndex == 0) {
            WrapperNum = UtilityRoutines::FindItemInList(WrapperName, Wrapper);
            if (WrapperNum == 0) {
                ShowFatalError("SimCentralGroundSourceHeatPump: Specified Wrapper not one of Valid Wrappers=" + WrapperName);
            }
            CompIndex = WrapperNum;
        } else {
            WrapperNum = CompIndex;
            if (WrapperNum > numWrappers || WrapperNum < 1) {
                ShowFatalError("SimCentralGroundSourceHeatPump:  Invalid CompIndex passed=" + General::TrimSigDigits(WrapperNum) +
                               ", Number of Units=" + General::TrimSigDigits(numWrappers) + ", Entered Unit name=" + WrapperName);
            }
            if (CheckEquipName(WrapperNum)) {
                if (WrapperName != Wrapper(WrapperNum).Name) {
                    ShowFatalError("SimCentralGroundSourceHeatPump:  Invalid CompIndex passed=" + General::TrimSigDigits(WrapperNum) +
                                   ", Unit name=" + WrapperName + ", stored Unit Name for that index=" + Wrapper(WrapperNum).Name);
                }
                CheckEquipName(WrapperNum) = false;
            }
        }

        if (InitLoopEquip) { // Initialization loop if not done
            InitWrapper(WrapperNum, RunFlag, FirstIteration, MyLoad, LoopNum);
            MinCap = 0.0;
            MaxCap = 0.0;
            OptCap = 0.0;
            if (LoopNum == Wrapper(WrapperNum).CWLoopNum) { // Chilled water loop
                SizeWrapper(WrapperNum);
                if (Wrapper(WrapperNum).ControlMode == SmartMixing) { // control mode is SmartMixing
                    for (int NumChillerHeater = 1; NumChillerHeater <= Wrapper(WrapperNum).ChillerHeaterNums; ++NumChillerHeater) {
                        MaxCap += Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling *
                                  Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).MaxPartLoadRatCooling;

                        OptCap += Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling *
                                  Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).OptPartLoadRatCooling;

                        MinCap += Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling *
                                  Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).MinPartLoadRatCooling;
                    }
                }
            } else if (LoopNum == Wrapper(WrapperNum).HWLoopNum) {    // Hot water loop
                if (Wrapper(WrapperNum).ControlMode == SmartMixing) { // control mode is SmartMixing
                    for (int NumChillerHeater = 1; NumChillerHeater <= Wrapper(WrapperNum).ChillerHeaterNums; ++NumChillerHeater) {
                        MaxCap += Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapClgHtg *
                                  Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).MaxPartLoadRatClgHtg;

                        OptCap += Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapClgHtg *
                                  Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).OptPartLoadRatClgHtg;

                        MinCap += Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapClgHtg *
                                  Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).MinPartLoadRatClgHtg;
                    }
                } // End of control mode determination
            }     // End of loop determination

            if (GetSizingFactor) {
                SizingFactor = 1.0; // Always equal to one now. The component may have its own sizing factor
            }

            return;

        } // End of initialization

        if (LoopNum != Wrapper(WrapperNum).GLHELoopNum) {

            InitWrapper(WrapperNum, RunFlag, FirstIteration, MyLoad, LoopNum);
            CalcWrapperModel(WrapperNum, MyLoad, RunFlag, FirstIteration, EquipFlowCtrl, LoopNum);

        } else if (LoopNum == Wrapper(WrapperNum).GLHELoopNum) {
            PlantUtilities::UpdateChillerComponentCondenserSide(LoopNum,
                                                                Wrapper(WrapperNum).GLHELoopSideNum,
                                                DataPlant::TypeOf_CentralGroundSourceHeatPump,
                                                Wrapper(WrapperNum).GLHEInletNodeNum,
                                                Wrapper(WrapperNum).GLHEOutletNodeNum,
                                                WrapperReport(WrapperNum).GLHERate,
                                                WrapperReport(WrapperNum).GLHEInletTemp,
                                                WrapperReport(WrapperNum).GLHEOutletTemp,
                                                WrapperReport(WrapperNum).GLHEmdot,
                                                FirstIteration);

            // Use the first chiller heater's evaporator capacity ratio to determine dominant load
            Wrapper(WrapperNum).SimulClgDominant = false;
            Wrapper(WrapperNum).SimulHtgDominant = false;
            if (Wrapper(WrapperNum).WrapperCoolingLoad > 0 && Wrapper(WrapperNum).WrapperHeatingLoad > 0) {
                Real64 SimulLoadRatio = Wrapper(WrapperNum).WrapperCoolingLoad / Wrapper(WrapperNum).WrapperHeatingLoad;
                if (SimulLoadRatio > Wrapper(WrapperNum).ChillerHeater(1).ClgHtgToCoolingCapRatio) {
                    Wrapper(WrapperNum).SimulClgDominant = true;
                    Wrapper(WrapperNum).SimulHtgDominant = false;
                } else {
                    Wrapper(WrapperNum).SimulHtgDominant = true;
                    Wrapper(WrapperNum).SimulClgDominant = false;
                }
            }
        }
    }

    void SizeWrapper(int const WrapperNum)
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Yunzhi Huang, PNNL
        //       DATE WRITTEN   Feb 2013
        //       MODIFIED       November 2013 Daeho Kang, add component sizing table entries
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        //  This subroutine is for sizing all the components under each 'CentralHeatPumpSystem' object,
        //  for which capacities and flow rates have not been specified in the input.

        // METHODOLOGY EMPLOYED:
        //  Obtains evaporator flow rate from the plant sizing array. Calculates reference capacity from
        //  the evaporator (or load side) flow rate and the chilled water loop design delta T. The condenser
        //  flow (or source side) rate is calculated from the reference capacity, the COP, and the condenser
        //  loop design delta T.

        static std::string const RoutineName("SizeCGSHPChillerHeater");

        bool ErrorsFound;  // If errors detected in input

        // auto-size the chiller heater components
        if (Wrapper(WrapperNum).ControlMode == SmartMixing) {

            for (int NumChillerHeater = 1; NumChillerHeater <= Wrapper(WrapperNum).ChillerHeaterNums; ++NumChillerHeater) {
                ErrorsFound = false;

                // find the appropriate Plant Sizing object
                int PltSizNum = DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).PlantSizNum;

                // if ( Wrapper( WrapperNum ).ChillerHeater( NumChillerHeater ).CondVolFlowRate == AutoSize ) {
                int PltSizCondNum = DataPlant::PlantLoop(Wrapper(WrapperNum).GLHELoopNum).PlantSizNum;
                //}

                Real64 tmpNomCap = Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling;
                Real64 tmpEvapVolFlowRate = Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRate;
                Real64 tmpCondVolFlowRate = Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRate;

                // auto-size the Evaporator Flow Rate
                if (PltSizNum > 0) {
                    if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= DataHVACGlobals::SmallWaterVolFlow) {
                        tmpEvapVolFlowRate = DataSizing::PlantSizData(PltSizNum).DesVolFlowRate * Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).SizFac;
                        Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).tmpEvapVolFlowRate = tmpEvapVolFlowRate;
                        if (!Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRateWasAutoSized)
                            tmpEvapVolFlowRate = Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRate;

                    } else {
                        if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRateWasAutoSized) tmpEvapVolFlowRate = 0.0;
                        Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).tmpEvapVolFlowRate = tmpEvapVolFlowRate;
                    }
                    if (DataPlant::PlantFirstSizesOkayToFinalize) {
                        if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRateWasAutoSized) {
                            Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRate = tmpEvapVolFlowRate;
                            if (DataPlant::PlantFinalSizesOkayToReport) {
                                ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                                   Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                                   "Design Size Reference Chilled Water Flow Rate [m3/s]",
                                                   tmpEvapVolFlowRate);
                            }
                            if (DataPlant::PlantFirstSizesOkayToReport) {
                                ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                                   Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                                   "Initial Design Size Reference Chilled Water Flow Rate [m3/s]",
                                                   tmpEvapVolFlowRate);
                            }
                        } else {
                            if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRate > 0.0 && tmpEvapVolFlowRate > 0.0 &&
                                    DataPlant::PlantFinalSizesOkayToReport) {

                                // Hardsized evaporator design volume flow rate for reporting
                                Real64 EvapVolFlowRateUser = Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRate;
                                ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                                   Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                                   "Design Size Reference Chilled Water Flow Rate [m3/s]",
                                                   tmpEvapVolFlowRate,
                                                   "User-Specified Reference Chilled Water Flow Rate [m3/s]",
                                                   EvapVolFlowRateUser);
                                tmpEvapVolFlowRate = EvapVolFlowRateUser;
                                if (DataGlobals::DisplayExtraWarnings) {
                                    if ((std::abs(tmpEvapVolFlowRate - EvapVolFlowRateUser) / EvapVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                        ShowMessage("SizeChillerHeaterPerformanceElectricEIR: Potential issue with equipment sizing for " +
                                                    Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name);
                                        ShowContinueError("User-Specified Reference Chilled Water Flow Rate of " +
                                                          General::RoundSigDigits(EvapVolFlowRateUser, 5) + " [m3/s]");
                                        ShowContinueError("differs from Design Size Reference Chilled Water Flow Rate of " +
                                                          General::RoundSigDigits(tmpEvapVolFlowRate, 5) + " [m3/s]");
                                        ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                        ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRateWasAutoSized) {
                        if (DataPlant::PlantFirstSizesOkayToFinalize) {
                            ShowSevereError("Autosizing of CGSHP Chiller Heater evap flow rate requires a loop Sizing:Plant object");
                            ShowContinueError("Occurs in CGSHP Chiller Heater Performance object=" +
                                              Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name);
                            ErrorsFound = true;
                        }
                    } else {
                        if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRate > 0.0 && DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                               Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                               "User-Specified Reference Chilled Water Flow Rate [m3/s]",
                                               Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).EvapVolFlowRate);
                        }
                    }
                }

                // auto-size the Reference Cooling Capacity
                // each individual chiller heater module is sized to be capable of supporting the total load on the wrapper
                if (PltSizNum > 0) {
                    if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= DataHVACGlobals::SmallWaterVolFlow && tmpEvapVolFlowRate > 0.0) {
                        Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidName,
                                                   DataGlobals::CWInitConvTemp,
                                                   DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidIndex,
                                                   RoutineName);

                        Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidName,
                                               DataGlobals::CWInitConvTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidIndex,
                                               RoutineName);
                        tmpNomCap = Cp * rho * DataSizing::PlantSizData(PltSizNum).DeltaT * tmpEvapVolFlowRate;
                        if (!Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCoolingWasAutoSized)
                            tmpNomCap = Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling;
                    } else {
                        if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCoolingWasAutoSized) tmpNomCap = 0.0;
                    }
                    if (DataPlant::PlantFirstSizesOkayToFinalize) {
                        if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCoolingWasAutoSized) {
                            Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling = tmpNomCap;

                            // Now that we have the Reference Cooling Capacity, we need to also initialize the Heating side
                            // given the ratios
                            Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapClgHtg =
                                Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling *
                                Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).ClgHtgToCoolingCapRatio;

                            Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefPowerClgHtg =
                                ( Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling
                                / Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCOPCooling )
                                * Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).ClgHtgtoCogPowerRatio;

                            Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCOPClgHtg =
                                Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapClgHtg
                                / Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefPowerClgHtg;

                            if (DataPlant::PlantFinalSizesOkayToReport) {
                                ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                                   Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                                   "Design Size Reference Capacity [W]",
                                                   tmpNomCap);
                            }
                            if (DataPlant::PlantFirstSizesOkayToReport) {
                                ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                                   Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                                   "Initial Design Size Reference Capacity [W]",
                                                   tmpNomCap);
                            }
                        } else {
                            if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling > 0.0 && tmpNomCap > 0.0 &&
                                    DataPlant::PlantFinalSizesOkayToReport) {

                                // Hardsized nominal capacity cooling power for reporting
                                Real64 NomCapUser = Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling;
                                ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                                   Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                                   "Design Size Reference Capacity [W]",
                                                   tmpNomCap,
                                                   "User-Specified Reference Capacity [W]",
                                                   NomCapUser);
                                tmpNomCap = NomCapUser;
                                if (DataGlobals::DisplayExtraWarnings) {
                                    if ((std::abs(tmpNomCap - NomCapUser) / NomCapUser) > DataSizing::AutoVsHardSizingThreshold) {
                                        ShowMessage("SizeChillerHeaterPerformanceElectricEIR: Potential issue with equipment sizing for " +
                                                    Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name);
                                        ShowContinueError("User-Specified Reference Capacity of " + General::RoundSigDigits(NomCapUser, 2) + " [W]");
                                        ShowContinueError("differs from Design Size Reference Capacity of " + General::RoundSigDigits(tmpNomCap, 2) + " [W]");
                                        ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                        ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCoolingWasAutoSized) {
                        if (DataPlant::PlantFirstSizesOkayToFinalize) {
                            ShowSevereError("Size ChillerHeaterPerformance:Electric:EIR=\"" +
                                            Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name + "\", autosize error.");
                            ShowContinueError("Autosizing of CGSHP Chiller Heater reference capacity requires");
                            ShowContinueError("a cooling loop Sizing:Plant object.");
                            ErrorsFound = true;
                        }
                    } else {
                        if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling > 0.0 && DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                               Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                               "User-Specified Reference Capacity [W]",
                                               Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling);
                        }
                    }
                }

                // auto-size the condenser volume flow rate
                // each individual chiller heater module is sized to be capable of supporting the total load on the wrapper
                if (PltSizCondNum > 0) {
                    if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= DataHVACGlobals::SmallWaterVolFlow) {
                        Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).GLHELoopNum).FluidName,
                                               DataGlobals::CWInitConvTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).GLHELoopNum).FluidIndex,
                                               RoutineName);
                        // TODO: JM 2018-12-06 I wonder why Cp isn't calculated at the same temp as rho...
                        Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).GLHELoopNum).FluidName,
                                                   Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).TempRefCondInCooling,
                                                   DataPlant::PlantLoop(Wrapper(WrapperNum).GLHELoopNum).FluidIndex,
                                                   RoutineName);
                        tmpCondVolFlowRate = tmpNomCap *
                                             (1.0 + (1.0 / Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCOPCooling) *
                                                        Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).OpenMotorEff) /
                                             (DataSizing::PlantSizData(PltSizCondNum).DeltaT * Cp * rho);
                        Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).tmpCondVolFlowRate = tmpCondVolFlowRate;
                        if (!Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRateWasAutoSized)
                            tmpCondVolFlowRate = Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRate;

                    } else {
                        if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRateWasAutoSized) tmpCondVolFlowRate = 0.0;
                        Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).tmpCondVolFlowRate = tmpCondVolFlowRate;
                    }
                    if (DataPlant::PlantFirstSizesOkayToFinalize) {
                        if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRateWasAutoSized) {
                            Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRate = tmpCondVolFlowRate;
                            if (DataPlant::PlantFinalSizesOkayToReport) {
                                ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                                   Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                                   "Design Size Reference Condenser Water Flow Rate [m3/s]",
                                                   tmpCondVolFlowRate);
                            }
                            if (DataPlant::PlantFirstSizesOkayToReport) {
                                ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                                   Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                                   "Initial Design Size Reference Condenser Water Flow Rate [m3/s]",
                                                   tmpCondVolFlowRate);
                            }
                        } else {
                            if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRate > 0.0 && tmpCondVolFlowRate > 0.0 &&
                                    DataPlant::PlantFinalSizesOkayToReport) {

                                // Hardsized condenser design volume flow rate for reporting
                                Real64 CondVolFlowRateUser = Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRate;
                                ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                                   Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                                   "Design Size Reference Condenser Water Flow Rate [m3/s]",
                                                   tmpCondVolFlowRate,
                                                   "User-Specified Reference Condenser Water Flow Rate [m3/s]",
                                                   CondVolFlowRateUser);
                                if (DataGlobals::DisplayExtraWarnings) {
                                    if ((std::abs(tmpCondVolFlowRate - CondVolFlowRateUser) / CondVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                        ShowMessage("SizeChillerHeaterPerformanceElectricEIR: Potential issue with equipment sizing for " +
                                                    Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name);
                                        ShowContinueError("User-Specified Reference Condenser Water Flow Rate of " +
                                                          General::RoundSigDigits(CondVolFlowRateUser, 5) + " [m3/s]");
                                        ShowContinueError("differs from Design Size Reference Condenser Water Flow Rate of " +
                                                          General::RoundSigDigits(tmpCondVolFlowRate, 5) + " [m3/s]");
                                        ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                        ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRateWasAutoSized) {
                        if (DataPlant::PlantFirstSizesOkayToFinalize) {
                            ShowSevereError("Size ChillerHeaterPerformance:Electric:EIR=\"" +
                                            Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name + "\", autosize error.");
                            ShowContinueError("Autosizing of CGSHP Chiller Heater condenser flow rate requires");
                            ShowContinueError("a condenser loop Sizing:Plant object.");
                            ErrorsFound = true;
                        }
                    } else {
                        if (Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRate > 0.0 && DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("ChillerHeaterPerformance:Electric:EIR",
                                               Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name,
                                               "User-Specified Reference Condenser Water Flow Rate [m3/s]",
                                               Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).CondVolFlowRate);
                        }
                    }
                }

                if (DataPlant::PlantFinalSizesOkayToReport) {
                    // create predefined report
                    std::string equipName = Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).Name;
                    OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechType, equipName, "ChillerHeaterPerformance:Electric:EIR");
                    OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomEff, equipName, Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCOPCooling);
                    OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomCap, equipName, Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).RefCapCooling);
                }

                if (ErrorsFound) {
                    ShowFatalError("Preceding sizing errors cause program termination");
                }
            }

            // sum individual volume flows and register wrapper inlets
            Real64 TotalEvapVolFlowRate = 0.0;
            Real64 TotalCondVolFlowRate = 0.0;
            Real64 TotalHotWaterVolFlowRate = 0.0;
            for (int NumChillerHeater = 1; NumChillerHeater <= Wrapper(WrapperNum).ChillerHeaterNums; ++NumChillerHeater) {
                TotalEvapVolFlowRate += Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).tmpEvapVolFlowRate;
                TotalCondVolFlowRate += Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).tmpCondVolFlowRate;
                TotalHotWaterVolFlowRate += Wrapper(WrapperNum).ChillerHeater(NumChillerHeater).DesignHotWaterVolFlowRate;
            }

            PlantUtilities::RegisterPlantCompDesignFlow(Wrapper(WrapperNum).CHWInletNodeNum, TotalEvapVolFlowRate);
            PlantUtilities::RegisterPlantCompDesignFlow(Wrapper(WrapperNum).HWInletNodeNum, TotalHotWaterVolFlowRate);
            // save the reference condenser water volumetric flow rate for use by the condenser water loop sizing algorithms
            PlantUtilities::RegisterPlantCompDesignFlow(Wrapper(WrapperNum).GLHEInletNodeNum, TotalCondVolFlowRate);

            return;
        }
    }

    void GetWrapperInput()
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Yunzhi Huang and Daeho Kang, PNNL
        //       DATE WRITTEN:    Feb 2013

        // PURPOSE OF THIS SUBROUTINE:
        //  This routine will get the input required by the Wrapper model.

        bool ErrorsFound(false);     // True when input errors are found
        int NumAlphas;                      // Number of elements in the alpha array
        int NumNums;                        // Number of elements in the numeric array
        int IOStat;                         // IO Status when calling get input subroutine

        DataIPShortCuts::cCurrentModuleObject = "CentralHeatPumpSystem";
        numWrappers = inputProcessor->getNumObjectsFound(DataIPShortCuts::cCurrentModuleObject);

        if (numWrappers <= 0) {
            ShowSevereError("No " + DataIPShortCuts::cCurrentModuleObject + " equipment specified in input file");
        }

        Wrapper.allocate(numWrappers);
        WrapperReport.allocate(numWrappers);
        CheckEquipName.dimension(numWrappers, true);

        // Load arrays with electric EIR chiller data
        for (int WrapperNum = 1; WrapperNum <= numWrappers; ++WrapperNum) {
            inputProcessor->getObjectItem(DataIPShortCuts::cCurrentModuleObject,
                                          WrapperNum,
                                          DataIPShortCuts::cAlphaArgs,
                                          NumAlphas,
                                          DataIPShortCuts::rNumericArgs,
                                          NumNums,
                                          IOStat,
                                          _,
                                          DataIPShortCuts::lAlphaFieldBlanks,
                                          DataIPShortCuts::cAlphaFieldNames,
                                          DataIPShortCuts::cNumericFieldNames);

            Wrapper(WrapperNum).Name = DataIPShortCuts::cAlphaArgs(1);

            // initialize nth chiller heater index (including identical units) for current wrapper
            int NumChHtrPerWrapper = 0;
            if (UtilityRoutines::IsNameEmpty(DataIPShortCuts::cAlphaArgs(1), DataIPShortCuts::cCurrentModuleObject, ErrorsFound)) {
                continue;
            }

            if (DataIPShortCuts::cAlphaArgs(2) == "SMARTMIXING") {
                Wrapper(WrapperNum).ControlMode = SmartMixing;
            }

            Wrapper(WrapperNum).CHWInletNodeNum = NodeInputManager::GetOnlySingleNode(DataIPShortCuts::cAlphaArgs(3),
                                                                    ErrorsFound,
                                                                    DataIPShortCuts::cCurrentModuleObject,
                                                                    DataIPShortCuts::cAlphaArgs(1),
                                                                    DataLoopNode::NodeType_Water,
                                                                    DataLoopNode::NodeConnectionType_Inlet,
                                                                    1,
                                                                    DataLoopNode::ObjectIsNotParent); // node name : connection should be careful!
            Wrapper(WrapperNum).CHWOutletNodeNum = NodeInputManager::GetOnlySingleNode(
                DataIPShortCuts::cAlphaArgs(4), ErrorsFound, DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Outlet, 1, DataLoopNode::ObjectIsNotParent);
            BranchNodeConnections::TestCompSet(DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs(1), DataIPShortCuts::cAlphaArgs(3), DataIPShortCuts::cAlphaArgs(4), "Chilled Water Nodes");

            Wrapper(WrapperNum).GLHEInletNodeNum = NodeInputManager::GetOnlySingleNode(DataIPShortCuts::cAlphaArgs(5),
                                                                     ErrorsFound,
                                                                     DataIPShortCuts::cCurrentModuleObject,
                                                                     DataIPShortCuts::cAlphaArgs(1),
                                                                     DataLoopNode::NodeType_Water,
                                                                     DataLoopNode::NodeConnectionType_Inlet,
                                                                     2,
                                                                     DataLoopNode::ObjectIsNotParent); // node name : connection should be careful!
            Wrapper(WrapperNum).GLHEOutletNodeNum = NodeInputManager::GetOnlySingleNode(
                DataIPShortCuts::cAlphaArgs(6), ErrorsFound, DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Outlet, 2, DataLoopNode::ObjectIsNotParent);
            BranchNodeConnections::TestCompSet(DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs(1), DataIPShortCuts::cAlphaArgs(5), DataIPShortCuts::cAlphaArgs(6), "GLHE Nodes");

            Wrapper(WrapperNum).HWInletNodeNum = NodeInputManager::GetOnlySingleNode(DataIPShortCuts::cAlphaArgs(7),
                                                                   ErrorsFound,
                                                                   DataIPShortCuts::cCurrentModuleObject,
                                                                   DataIPShortCuts::cAlphaArgs(1),
                                                                   DataLoopNode::NodeType_Water,
                                                                   DataLoopNode::NodeConnectionType_Inlet,
                                                                   3,
                                                                   DataLoopNode::ObjectIsNotParent); // node name : connection should be careful!
            Wrapper(WrapperNum).HWOutletNodeNum = NodeInputManager::GetOnlySingleNode(
                DataIPShortCuts::cAlphaArgs(8), ErrorsFound, DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Outlet, 3, DataLoopNode::ObjectIsNotParent);
            BranchNodeConnections::TestCompSet(DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs(1), DataIPShortCuts::cAlphaArgs(7), DataIPShortCuts::cAlphaArgs(8), "Hot Water Nodes");

            Wrapper(WrapperNum).AncillaryPower = DataIPShortCuts::rNumericArgs(1);
            if (DataIPShortCuts::lAlphaFieldBlanks(9)) {
                Wrapper(WrapperNum).SchedPtr = 0;
            } else {
                Wrapper(WrapperNum).SchedPtr = ScheduleManager::GetScheduleIndex(DataIPShortCuts::cAlphaArgs(9));
            }

            int NumberOfComp = (NumAlphas - 9) / 3;
            Wrapper(WrapperNum).NumOfComp = NumberOfComp;
            Wrapper(WrapperNum).WrapperComp.allocate(NumberOfComp);

            if (Wrapper(WrapperNum).NumOfComp == 0) {
                ShowSevereError("GetWrapperInput: No component names on " + DataIPShortCuts::cCurrentModuleObject + '=' + Wrapper(WrapperNum).Name);
                ErrorsFound = true;
            } else {
                int Comp = 0;
                for (int loop = 10; loop <= NumAlphas; loop += 3) {
                    ++Comp;
                    Wrapper(WrapperNum).WrapperComp(Comp).WrapperPerformanceObjectType = DataIPShortCuts::cAlphaArgs(loop);
                    Wrapper(WrapperNum).WrapperComp(Comp).WrapperComponentName = DataIPShortCuts::cAlphaArgs(loop + 1);
                    if (DataIPShortCuts::lAlphaFieldBlanks(loop + 2)) {
                        Wrapper(WrapperNum).WrapperComp(Comp).CHSchedPtr = DataGlobals::ScheduleAlwaysOn;
                    } else {
                        Wrapper(WrapperNum).WrapperComp(Comp).CHSchedPtr = ScheduleManager::GetScheduleIndex(DataIPShortCuts::cAlphaArgs(loop + 2));
                    }
                    Wrapper(WrapperNum).WrapperComp(Comp).WrapperIdenticalObjectNum = DataIPShortCuts::rNumericArgs(1 + Comp);
                    if (Wrapper(WrapperNum).WrapperComp(Comp).WrapperPerformanceObjectType == "CHILLERHEATERPERFORMANCE:ELECTRIC:EIR") {

                        // count number of chiller heaters (including identical units) for current wrapper
                        if (Wrapper(WrapperNum).WrapperComp(Comp).WrapperIdenticalObjectNum > 1) {
                            NumChHtrPerWrapper += Wrapper(WrapperNum).WrapperComp(Comp).WrapperIdenticalObjectNum;
                        } else {
                            ++NumChHtrPerWrapper;
                        }

                        // count total number of chiller heaters (not including identical units) for ALL wrappers
                        ++numChillerHeaters;
                    }
                }

                Wrapper(WrapperNum).ChillerHeaterNums = NumChHtrPerWrapper;
            }

            if (ErrorsFound) {
                ShowFatalError("GetWrapperInput: Invalid " + DataIPShortCuts::cCurrentModuleObject + " Input, preceding condition(s) cause termination.");
            }

            // ALLOCATE ARRAYS
            if ((numChillerHeaters == 0) && (Wrapper(WrapperNum).ControlMode == SmartMixing)) {
                ShowFatalError("SmartMixing Control Mode in object " + DataIPShortCuts::cCurrentModuleObject + " : " + Wrapper(WrapperNum).Name +
                               " need to apply to ChillerHeaterPerformance:Electric:EIR object(s).");
            }
        }

        if (numChillerHeaters > 0) {

            for (int WrapperNum = 1; WrapperNum <= numWrappers; ++WrapperNum) {
                Wrapper(WrapperNum).ChillerHeater.allocate(Wrapper(WrapperNum).ChillerHeaterNums);
                Wrapper(WrapperNum).ChillerHeaterReport.allocate(Wrapper(WrapperNum).ChillerHeaterNums);
            }
            GetChillerHeaterInput();
        }

        for (int WrapperNum = 1; WrapperNum <= numWrappers; ++WrapperNum) {
            int ChillerHeaterNum = 0; // initialize nth chiller heater index (including identical units) for current wrapper
            for (int Comp = 1; Comp <= Wrapper(WrapperNum).NumOfComp; ++Comp) {
                if (Wrapper(WrapperNum).WrapperComp(Comp).WrapperPerformanceObjectType == "CHILLERHEATERPERFORMANCE:ELECTRIC:EIR") {
                    std::string CompName = Wrapper(WrapperNum).WrapperComp(Comp).WrapperComponentName;
                    int CompIndex = UtilityRoutines::FindItemInList(CompName, ChillerHeater);
                    // User may enter invalid name rather than selecting one from the object list
                    if (CompIndex <= 0) {
                        ShowSevereError("GetWrapperInput: Invalid Chiller Heater Modules Performance Component Name =" + CompName);
                        ShowContinueError("Select the name of ChillerHeaterPerformance:Electric:EIR object(s) from the object list.");
                        ShowFatalError("Program terminates due to preceding condition.");
                    }
                    Wrapper(WrapperNum).WrapperComp(Comp).WrapperPerformanceObjectIndex = CompIndex;
                    if (ChillerHeater(CompIndex).VariableFlow) {
                        Wrapper(WrapperNum).VariableFlowCH = true;
                    }
                    for (int i_CH = 1; i_CH <= Wrapper(WrapperNum).WrapperComp(Comp).WrapperIdenticalObjectNum; ++i_CH) {
                        // increment nth chiller heater index (including identical units) for current wrapper
                        ++ChillerHeaterNum;
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum) = ChillerHeater(CompIndex);
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum) = ChillerHeaterReport(CompIndex);
                    }
                }
            }
        }

        // Release memory from temporary arrays; values now copied into their associated Wrapper in above loop
        if (allocated(ChillerHeater)) ChillerHeater.deallocate();
        if (allocated(ChillerHeaterReport)) ChillerHeaterReport.deallocate();

        // Set up output variables
        for (int WrapperNum = 1; WrapperNum <= numWrappers; ++WrapperNum) {
            SetupOutputVariable("Chiller Heater System Cooling Electric Energy",
                                OutputProcessor::Unit::J,
                                WrapperReport(WrapperNum).TotElecCooling,
                                "System",
                                "Sum",
                                Wrapper(WrapperNum).Name,
                                _,
                                "ELECTRICITY",
                                "Cooling",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater System Heating Electric Energy",
                                OutputProcessor::Unit::J,
                                WrapperReport(WrapperNum).TotElecHeating,
                                "System",
                                "Sum",
                                Wrapper(WrapperNum).Name,
                                _,
                                "ELECTRICITY",
                                "Heating",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater System Cooling Electric Power",
                                OutputProcessor::Unit::W,
                                WrapperReport(WrapperNum).TotElecCoolingPwr,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Heating Electric Power",
                                OutputProcessor::Unit::W,
                                WrapperReport(WrapperNum).TotElecHeatingPwr,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Cooling Energy",
                                OutputProcessor::Unit::J,
                                WrapperReport(WrapperNum).CoolingEnergy,
                                "System",
                                "Sum",
                                Wrapper(WrapperNum).Name,
                                _,
                                "ENERGYTRANSFER",
                                "CHILLERS",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater System Heating Energy",
                                OutputProcessor::Unit::J,
                                WrapperReport(WrapperNum).HeatingEnergy,
                                "System",
                                "Sum",
                                Wrapper(WrapperNum).Name,
                                _,
                                "ENERGYTRANSFER",
                                "BOILER",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater System Source Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                WrapperReport(WrapperNum).GLHEEnergy,
                                "System",
                                "Sum",
                                Wrapper(WrapperNum).Name,
                                _,
                                "ENERGYTRANSFER",
                                "HEATREJECTION",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater System Cooling Rate",
                                OutputProcessor::Unit::W,
                                WrapperReport(WrapperNum).CoolingRate,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Heating Rate",
                                OutputProcessor::Unit::W,
                                WrapperReport(WrapperNum).HeatingRate,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Source Heat Transfer Rate",
                                OutputProcessor::Unit::W,
                                WrapperReport(WrapperNum).GLHERate,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Cooling Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                WrapperReport(WrapperNum).CHWmdot,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Heating Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                WrapperReport(WrapperNum).HWmdot,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Source Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                WrapperReport(WrapperNum).GLHEmdot,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Cooling Inlet Temperature",
                                OutputProcessor::Unit::C,
                                WrapperReport(WrapperNum).CHWInletTemp,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Heating Inlet Temperature",
                                OutputProcessor::Unit::C,
                                WrapperReport(WrapperNum).HWInletTemp,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Source Inlet Temperature",
                                OutputProcessor::Unit::C,
                                WrapperReport(WrapperNum).GLHEInletTemp,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Cooling Outlet Temperature",
                                OutputProcessor::Unit::C,
                                WrapperReport(WrapperNum).CHWOutletTemp,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Heating Outlet Temperature",
                                OutputProcessor::Unit::C,
                                WrapperReport(WrapperNum).HWOutletTemp,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            SetupOutputVariable("Chiller Heater System Source Outlet Temperature",
                                OutputProcessor::Unit::C,
                                WrapperReport(WrapperNum).GLHEOutletTemp,
                                "System",
                                "Average",
                                Wrapper(WrapperNum).Name);

            if (Wrapper(WrapperNum).ChillerHeaterNums > 0) {

                for (int ChillerHeaterNum = 1; ChillerHeaterNum <= Wrapper(WrapperNum).ChillerHeaterNums; ++ChillerHeaterNum) {

                    SetupOutputVariable("Chiller Heater Operation Mode Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::None,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CurrentMode,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Part Load Ratio Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::None,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerPartLoadRatio,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Cycling Ratio Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::None,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCyclingRatio,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Cooling Electric Power Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::W,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPower,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Heating Electric Power Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::W,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingPower,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Cooling Electric Energy Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::J,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingEnergy,
                                        "System",
                                        "Sum",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Heating Electric Energy Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::J,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingEnergy,
                                        "System",
                                        "Sum",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Cooling Rate Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::W,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvap,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Cooling Energy Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::J,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergy,
                                        "System",
                                        "Sum",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater False Load Heat Transfer Rate Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::W,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoadRate,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater False Load Heat Transfer Energy Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::J,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoad,
                                        "System",
                                        "Sum",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Evaporator Inlet Temperature Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::C,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapInletTemp,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Evaporator Outlet Temperature Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::C,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTemp,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Evaporator Mass Flow Rate Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::kg_s,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Evapmdot,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Condenser Heat Transfer Rate Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::W,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCond,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Condenser Heat Transfer Energy Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::J,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergy,
                                        "System",
                                        "Sum",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater COP Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::W_W,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ActualCOP,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Capacity Temperature Modifier Multiplier Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::None,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCapFT,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater EIR Temperature Modifier Multiplier Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::None,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFT,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater EIR Part Load Modifier Multiplier Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::None,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFPLR,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Condenser Inlet Temperature Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::C,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondInletTemp,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Condenser Outlet Temperature Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::C,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTemp,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);

                    SetupOutputVariable("Chiller Heater Condenser Mass Flow Rate Unit " + General::TrimSigDigits(ChillerHeaterNum) + "",
                                        OutputProcessor::Unit::kg_s,
                                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot,
                                        "System",
                                        "Average",
                                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);
                } // End of individual chiller heater count for current wrapper

            } // End of individual chiller heater output

        } // End of wrapper count
    }

    void GetChillerHeaterInput()
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Kyung Tae Yun, Mississippi State University
        //       DATE WRITTEN:    Feb 2013

        // PURPOSE OF THIS SUBROUTINE:
        //  This routine will get the input required by the ChillerHeaterPerformance:Electric:EIR model.

        std::string StringVar;             // Used for EIRFPLR warning messages
        bool CHErrorsFound(false);  // True when input errors are found
        bool FoundNegValue(false);  // Used to evaluate PLFFPLR curve objects
        int NumAlphas;                     // Number of elements in the alpha array
        int NumNums;                       // Number of elements in the numeric array
        int IOStat;                        // IO Status when calling get input subroutine
        Array1D<Real64> CurveValArray(11); // Used to evaluate PLFFPLR curve objects

        static ObjexxFCL::gio::Fmt Format_530("('Curve Output = ',11(F7.2))");
        static ObjexxFCL::gio::Fmt Format_550("('Curve Output = ',11(F7.2))");

        DataIPShortCuts::cCurrentModuleObject = "ChillerHeaterPerformance:Electric:EIR";
        numChillerHeaters = inputProcessor->getNumObjectsFound(DataIPShortCuts::cCurrentModuleObject);

        if (numChillerHeaters <= 0) {
            ShowSevereError("No " + DataIPShortCuts::cCurrentModuleObject + " equipment specified in input file");
            CHErrorsFound = true;
        }

        // Allocate temporary ChillerHeater and ChillerHeaterReport arrays
        if (allocated(ChillerHeater)) ChillerHeater.deallocate();
        if (allocated(ChillerHeaterReport)) ChillerHeaterReport.deallocate();
        ChillerHeater.allocate(numChillerHeaters);
        ChillerHeaterReport.allocate(numChillerHeaters);

        // Load arrays with electric EIR chiller data
        for (int ChillerHeaterNum = 1; ChillerHeaterNum <= numChillerHeaters; ++ChillerHeaterNum) {
            inputProcessor->getObjectItem(DataIPShortCuts::cCurrentModuleObject,
                                          ChillerHeaterNum,
                                          DataIPShortCuts::cAlphaArgs,
                                          NumAlphas,
                                          DataIPShortCuts::rNumericArgs,
                                          NumNums,
                                          IOStat,
                                          _,
                                          DataIPShortCuts::lAlphaFieldBlanks,
                                          DataIPShortCuts::cAlphaFieldNames,
                                          DataIPShortCuts::cNumericFieldNames);

            ChillerHeater(ChillerHeaterNum).Name = DataIPShortCuts::cAlphaArgs(1);
            UtilityRoutines::IsNameEmpty(DataIPShortCuts::cAlphaArgs(1), DataIPShortCuts::cCurrentModuleObject, CHErrorsFound);

            ChillerHeater(ChillerHeaterNum).CondModeCooling = DataIPShortCuts::cAlphaArgs(4);

            // Performance curves
            ChillerHeater(ChillerHeaterNum).ChillerCapFTCoolingIDX = CurveManager::GetCurveIndex(DataIPShortCuts::cAlphaArgs(5));
            if (ChillerHeater(ChillerHeaterNum).ChillerCapFTCoolingIDX == 0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cAlphaFieldNames(5) + '=' + DataIPShortCuts::cAlphaArgs(5));
                CHErrorsFound = true;
            }

            ChillerHeater(ChillerHeaterNum).ChillerEIRFTCoolingIDX = CurveManager::GetCurveIndex(DataIPShortCuts::cAlphaArgs(6));
            if (ChillerHeater(ChillerHeaterNum).ChillerEIRFTCoolingIDX == 0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cAlphaFieldNames(6) + '=' + DataIPShortCuts::cAlphaArgs(6));
                CHErrorsFound = true;
            }

            ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRCoolingIDX = CurveManager::GetCurveIndex(DataIPShortCuts::cAlphaArgs(7));
            if (ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRCoolingIDX == 0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cAlphaFieldNames(7) + '=' + DataIPShortCuts::cAlphaArgs(7));
                CHErrorsFound = true;
            }

            ChillerHeater(ChillerHeaterNum).CondModeHeating = DataIPShortCuts::cAlphaArgs(8);

            // Performance curves
            ChillerHeater(ChillerHeaterNum).ChillerCapFTHeatingIDX = CurveManager::GetCurveIndex(DataIPShortCuts::cAlphaArgs(9));
            if (ChillerHeater(ChillerHeaterNum).ChillerCapFTHeatingIDX == 0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cAlphaFieldNames(9) + '=' + DataIPShortCuts::cAlphaArgs(9));
                CHErrorsFound = true;
            }

            ChillerHeater(ChillerHeaterNum).ChillerEIRFTHeatingIDX = CurveManager::GetCurveIndex(DataIPShortCuts::cAlphaArgs(10));
            if (ChillerHeater(ChillerHeaterNum).ChillerEIRFTHeatingIDX == 0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cAlphaFieldNames(10) + '=' + DataIPShortCuts::cAlphaArgs(10));
                CHErrorsFound = true;
            }

            ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRHeatingIDX = CurveManager::GetCurveIndex(DataIPShortCuts::cAlphaArgs(11));
            if (ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRHeatingIDX == 0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cAlphaFieldNames(11) + '=' + DataIPShortCuts::cAlphaArgs(11));
                CHErrorsFound = true;
            }

            if (DataIPShortCuts::cAlphaArgs(2) == "CONSTANTFLOW") {
                ChillerHeater(ChillerHeaterNum).ConstantFlow = true;
                ChillerHeater(ChillerHeaterNum).VariableFlow = false;
            } else if (DataIPShortCuts::cAlphaArgs(2) == "VARIABLEFLOW") {
                ChillerHeater(ChillerHeaterNum).ConstantFlow = false;
                ChillerHeater(ChillerHeaterNum).VariableFlow = true;
            } else { // Assume a constant flow chiller if none is specified
                ChillerHeater(ChillerHeaterNum).ConstantFlow = true;
                ChillerHeater(ChillerHeaterNum).VariableFlow = false;
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cAlphaFieldNames(2) + '=' + DataIPShortCuts::cAlphaArgs(2));
                ShowContinueError("simulation assumes CONSTANTFLOW and continues..");
            }

            if (ChillerHeaterNum > 1) {
                if (ChillerHeater(ChillerHeaterNum).ConstantFlow != ChillerHeater(ChillerHeaterNum - 1).ConstantFlow) {
                    ChillerHeater(ChillerHeaterNum).ConstantFlow = true;
                    ShowWarningError("Water flow mode is different from the other chiller heater(s) " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("Entered in " + DataIPShortCuts::cAlphaFieldNames(2) + '=' + DataIPShortCuts::cAlphaArgs(2));
                    ShowContinueError("Simulation assumes CONSTANTFLOW and continues..");
                }
            }

            if (UtilityRoutines::SameString(DataIPShortCuts::cAlphaArgs(3), "WaterCooled")) {
                ChillerHeater(ChillerHeaterNum).CondenserType = WaterCooled;
            } else {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cAlphaFieldNames(3) + '=' + DataIPShortCuts::cAlphaArgs(3));
                ShowContinueError("Valid entries is WaterCooled");
                CHErrorsFound = true;
            }

            // Chiller rated performance data
            ChillerHeater(ChillerHeaterNum).RefCapCooling = DataIPShortCuts::rNumericArgs(1);
            if (ChillerHeater(ChillerHeaterNum).RefCapCooling == DataSizing::AutoSize) {
                ChillerHeater(ChillerHeaterNum).RefCapCoolingWasAutoSized = true;
            }
            if (DataIPShortCuts::rNumericArgs(1) == 0.0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cNumericFieldNames(1) + '=' + General::RoundSigDigits(DataIPShortCuts::rNumericArgs(1), 2));
                CHErrorsFound = true;
            }
            ChillerHeater(ChillerHeaterNum).RefCOPCooling = DataIPShortCuts::rNumericArgs(2);
            if (DataIPShortCuts::rNumericArgs(2) == 0.0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cNumericFieldNames(2) + '=' + General::RoundSigDigits(DataIPShortCuts::rNumericArgs(2), 2));
                CHErrorsFound = true;
            }

            ChillerHeater(ChillerHeaterNum).TempRefEvapOutCooling = DataIPShortCuts::rNumericArgs(3);
            ChillerHeater(ChillerHeaterNum).TempRefCondInCooling = DataIPShortCuts::rNumericArgs(4);
            ChillerHeater(ChillerHeaterNum).TempRefCondOutCooling = DataIPShortCuts::rNumericArgs(5);

            // Reference Heating Mode Ratios for Capacity and Power
            ChillerHeater(ChillerHeaterNum).ClgHtgToCoolingCapRatio = DataIPShortCuts::rNumericArgs(6);
            if (DataIPShortCuts::rNumericArgs(6) == 0.0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cNumericFieldNames(6) + '=' + General::RoundSigDigits(DataIPShortCuts::rNumericArgs(6), 2));
                CHErrorsFound = true;
            }

            ChillerHeater(ChillerHeaterNum).ClgHtgtoCogPowerRatio = DataIPShortCuts::rNumericArgs(7);
            if (DataIPShortCuts::rNumericArgs(7) == 0.0) {
                ShowSevereError("Invalid " + DataIPShortCuts::cCurrentModuleObject + '=' + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError("Entered in " + DataIPShortCuts::cNumericFieldNames(7) + '=' + General::RoundSigDigits(DataIPShortCuts::rNumericArgs(7), 2));
                CHErrorsFound = true;
            }


            if (!ChillerHeater(ChillerHeaterNum).RefCapCoolingWasAutoSized) {
                ChillerHeater(ChillerHeaterNum).RefCapClgHtg = ChillerHeater(ChillerHeaterNum).ClgHtgToCoolingCapRatio
                                                             * ChillerHeater(ChillerHeaterNum).RefCapCooling;
                ChillerHeater(ChillerHeaterNum).RefPowerClgHtg =( ChillerHeater(ChillerHeaterNum).RefCapCooling
                                                                / ChillerHeater(ChillerHeaterNum).RefCOPCooling )
                                                                * ChillerHeater(ChillerHeaterNum).ClgHtgtoCogPowerRatio;
                ChillerHeater(ChillerHeaterNum).RefCOPClgHtg = ChillerHeater(ChillerHeaterNum).RefCapClgHtg
                                                             / ChillerHeater(ChillerHeaterNum).RefPowerClgHtg;
            }


            ChillerHeater(ChillerHeaterNum).TempRefEvapOutClgHtg = DataIPShortCuts::rNumericArgs(8);
            ChillerHeater(ChillerHeaterNum).TempRefCondOutClgHtg = DataIPShortCuts::rNumericArgs(9);
            ChillerHeater(ChillerHeaterNum).TempRefCondInClgHtg = DataIPShortCuts::rNumericArgs(10);
            ChillerHeater(ChillerHeaterNum).TempLowLimitEvapOut = DataIPShortCuts::rNumericArgs(11);
            ChillerHeater(ChillerHeaterNum).EvapVolFlowRate = DataIPShortCuts::rNumericArgs(12);
            if (ChillerHeater(ChillerHeaterNum).EvapVolFlowRate == DataSizing::AutoSize) {
                ChillerHeater(ChillerHeaterNum).EvapVolFlowRateWasAutoSized = true;
            }
            ChillerHeater(ChillerHeaterNum).CondVolFlowRate = DataIPShortCuts::rNumericArgs(13);
            if (ChillerHeater(ChillerHeaterNum).CondVolFlowRate == DataSizing::AutoSize) {
                ChillerHeater(ChillerHeaterNum).CondVolFlowRateWasAutoSized = true;
            }
            ChillerHeater(ChillerHeaterNum).DesignHotWaterVolFlowRate = DataIPShortCuts::rNumericArgs(14);
            ChillerHeater(ChillerHeaterNum).OpenMotorEff = DataIPShortCuts::rNumericArgs(15);
            ChillerHeater(ChillerHeaterNum).OptPartLoadRatCooling = DataIPShortCuts::rNumericArgs(16);
            ChillerHeater(ChillerHeaterNum).OptPartLoadRatClgHtg = DataIPShortCuts::rNumericArgs(17);
            ChillerHeater(ChillerHeaterNum).SizFac = DataIPShortCuts::rNumericArgs(18);

            if (ChillerHeater(ChillerHeaterNum).SizFac <= 0.0) ChillerHeater(ChillerHeaterNum).SizFac = 1.0;

            if (ChillerHeater(ChillerHeaterNum).OpenMotorEff < 0.0 || ChillerHeater(ChillerHeaterNum).OpenMotorEff > 1.0) {
                ShowSevereError("GetCurveInput: For " + DataIPShortCuts::cCurrentModuleObject + ": " + DataIPShortCuts::cAlphaArgs(1));
                ShowContinueError(DataIPShortCuts::cNumericFieldNames(14) + " = " + General::RoundSigDigits(DataIPShortCuts::rNumericArgs(14), 3));
                ShowContinueError(DataIPShortCuts::cNumericFieldNames(14) + " must be greater than or equal to zero");
                ShowContinueError(DataIPShortCuts::cNumericFieldNames(14) + " must be less than or equal to one");
                CHErrorsFound = true;
            }

            // Check the CAP-FT, EIR-FT, and PLR curves and warn user if different from 1.0 by more than +-10%
            if (ChillerHeater(ChillerHeaterNum).ChillerCapFTCoolingIDX > 0) {
                Real64 CurveVal = CurveManager::CurveValue(ChillerHeater(ChillerHeaterNum).ChillerCapFTCoolingIDX,
                                      ChillerHeater(ChillerHeaterNum).TempRefEvapOutCooling,
                                      ChillerHeater(ChillerHeaterNum).TempRefCondInCooling);
                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError("Capacity ratio as a function of temperature curve output is not equal to 1.0");
                    ShowContinueError("(+ or - 10%) at reference conditions for " + DataIPShortCuts::cCurrentModuleObject + "= " + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("Curve output at reference conditions = " + General::TrimSigDigits(CurveVal, 3));
                }
            }

            if (ChillerHeater(ChillerHeaterNum).ChillerEIRFTCoolingIDX > 0) {
                Real64 CurveVal = CurveManager::CurveValue(ChillerHeater(ChillerHeaterNum).ChillerEIRFTCoolingIDX,
                                      ChillerHeater(ChillerHeaterNum).TempRefEvapOutCooling,
                                      ChillerHeater(ChillerHeaterNum).TempRefCondInCooling);
                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError("Energy input ratio as a function of temperature curve output is not equal to 1.0");
                    ShowContinueError("(+ or - 10%) at reference conditions for " + DataIPShortCuts::cCurrentModuleObject + "= " + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("Curve output at reference conditions = " + General::TrimSigDigits(CurveVal, 3));
                }
            }

            if (ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRCoolingIDX > 0) {
                Real64 CurveVal = CurveManager::CurveValue(ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRCoolingIDX, 1.0);

                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError("Energy input ratio as a function of part-load ratio curve output is not equal to 1.0");
                    ShowContinueError("(+ or - 10%) at reference conditions for " + DataIPShortCuts::cCurrentModuleObject + "= " + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("Curve output at reference conditions = " + General::TrimSigDigits(CurveVal, 3));
                }
            }

            if (ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRCoolingIDX > 0) {
                FoundNegValue = false;
                for (int CurveCheck = 0; CurveCheck <= 10; ++CurveCheck) {
                    Real64 CurveValTmp = CurveManager::CurveValue(ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRCoolingIDX, double(CurveCheck / 10.0));
                    if (CurveValTmp < 0.0) FoundNegValue = true;
                    CurveValArray(CurveCheck + 1) = int(CurveValTmp * 100.0) / 100.0;
                }
                if (FoundNegValue) {
                    ShowWarningError("Energy input ratio as a function of part-load ratio curve shows negative values ");
                    ShowContinueError("for " + DataIPShortCuts::cCurrentModuleObject + "= " + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("EIR as a function of PLR curve output at various part-load ratios shown below:");
                    ShowContinueError("PLR   =  0.00   0.10   0.20   0.30   0.40   0.50   0.60   0.70   0.80   0.90   1.00");
                    ObjexxFCL::gio::write(StringVar, "'Curve Output = '");
                    for (int CurveValPtr = 1; CurveValPtr <= 11; ++CurveValPtr) {
                        ObjexxFCL::gio::write(StringVar, "(F7.2,$)") << CurveValArray(CurveValPtr);
                    }
                    ObjexxFCL::gio::write(StringVar);
                    ShowContinueError(StringVar);
                    CHErrorsFound = true;
                }
            }

            if (ChillerHeater(ChillerHeaterNum).ChillerCapFTHeatingIDX > 0) {
                Real64 CurveVal = CurveManager::CurveValue(ChillerHeater(ChillerHeaterNum).ChillerCapFTHeatingIDX,
                                      ChillerHeater(ChillerHeaterNum).TempRefEvapOutClgHtg,
                                      ChillerHeater(ChillerHeaterNum).TempRefCondInClgHtg);
                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError("Capacity ratio as a function of temperature curve output is not equal to 1.0");
                    ShowContinueError("(+ or - 10%) at reference conditions for " + DataIPShortCuts::cCurrentModuleObject + "= " + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("Curve output at reference conditions = " + General::TrimSigDigits(CurveVal, 3));
                }
            }

            if (ChillerHeater(ChillerHeaterNum).ChillerEIRFTHeatingIDX > 0) {
                Real64 CurveVal = CurveManager::CurveValue(ChillerHeater(ChillerHeaterNum).ChillerEIRFTHeatingIDX,
                                      ChillerHeater(ChillerHeaterNum).TempRefEvapOutClgHtg,
                                      ChillerHeater(ChillerHeaterNum).TempRefCondInClgHtg);
                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError("Energy input ratio as a function of temperature curve output is not equal to 1.0");
                    ShowContinueError("(+ or - 10%) at reference conditions for " + DataIPShortCuts::cCurrentModuleObject + "= " + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("Curve output at reference conditions = " + General::TrimSigDigits(CurveVal, 3));
                }
            }

            if (ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRHeatingIDX > 0) {
                Real64 CurveVal = CurveManager::CurveValue(ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRHeatingIDX, 1.0);

                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError("Energy input ratio as a function of part-load ratio curve output is not equal to 1.0");
                    ShowContinueError("(+ or - 10%) at reference conditions for " + DataIPShortCuts::cCurrentModuleObject + "= " + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("Curve output at reference conditions = " + General::TrimSigDigits(CurveVal, 3));
                }
            }

            if (ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRHeatingIDX > 0) {
                FoundNegValue = false;
                for (int CurveCheck = 0; CurveCheck <= 10; ++CurveCheck) {
                    Real64 CurveValTmp = CurveManager::CurveValue(ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRHeatingIDX, double(CurveCheck / 10.0));
                    if (CurveValTmp < 0.0) FoundNegValue = true;
                    CurveValArray(CurveCheck + 1) = int(CurveValTmp * 100.0) / 100.0;
                }
                if (FoundNegValue) {
                    ShowWarningError("Energy input ratio as a function of part-load ratio curve shows negative values ");
                    ShowContinueError("for " + DataIPShortCuts::cCurrentModuleObject + "= " + DataIPShortCuts::cAlphaArgs(1));
                    ShowContinueError("EIR as a function of PLR curve output at various part-load ratios shown below:");
                    ShowContinueError("PLR          =    0.00   0.10   0.20   0.30   0.40   0.50   0.60   0.70   0.80   0.90   1.00");
                    ObjexxFCL::gio::write(StringVar, "'Curve Output = '");
                    for (int CurveValPtr = 1; CurveValPtr <= 11; ++CurveValPtr) {
                        ObjexxFCL::gio::write(StringVar, "(F7.2,$)") << CurveValArray(CurveValPtr);
                    }
                    ObjexxFCL::gio::write(StringVar);
                    ShowContinueError(StringVar);
                    CHErrorsFound = true;
                }
            }

            CurveManager::GetCurveMinMaxValues(ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRHeatingIDX,
                                 ChillerHeater(ChillerHeaterNum).MinPartLoadRatClgHtg,
                                 ChillerHeater(ChillerHeaterNum).MaxPartLoadRatClgHtg);

            CurveManager::GetCurveMinMaxValues(ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRCoolingIDX,
                                 ChillerHeater(ChillerHeaterNum).MinPartLoadRatCooling,
                                 ChillerHeater(ChillerHeaterNum).MaxPartLoadRatCooling);
        }

        if (CHErrorsFound) {
            ShowFatalError("Errors found in processing input for " + DataIPShortCuts::cCurrentModuleObject);
        }
    }

    void InitWrapper(int const WrapperNum,                 // Number of the current wrapper being simulated
                     bool const EP_UNUSED(RunFlag),        // TRUE when chiller operating
                     bool const EP_UNUSED(FirstIteration), // Initialize variables when TRUE
                     Real64 const MyLoad,                  // Demand Load
                     int const LoopNum                     // Loop Number Index
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Daeho Kang, PNNL
        //       DATE WRITTEN   Feb 2013
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        //  This subroutine is for initializations of the CentralHeatPumpSystem variables

        // METHODOLOGY EMPLOYED:
        //  Uses the status flags to trigger initializations.

        static std::string const RoutineName("InitCGSHPHeatPump");

        if (Wrapper(WrapperNum).MyWrapperFlag) {
            // Locate the chillers on the plant loops for later usage
            bool errFlag = false;
            PlantUtilities::ScanPlantLoopsForObject(Wrapper(WrapperNum).Name,
                                    DataPlant::TypeOf_CentralGroundSourceHeatPump,
                                    Wrapper(WrapperNum).CWLoopNum,
                                    Wrapper(WrapperNum).CWLoopSideNum,
                                    Wrapper(WrapperNum).CWBranchNum,
                                    Wrapper(WrapperNum).CWCompNum,
                                    errFlag,
                                    _,
                                    _,
                                    _,
                                    Wrapper(WrapperNum).CHWInletNodeNum,
                                    _);

            PlantUtilities::ScanPlantLoopsForObject(Wrapper(WrapperNum).Name,
                                    DataPlant::TypeOf_CentralGroundSourceHeatPump,
                                    Wrapper(WrapperNum).HWLoopNum,
                                    Wrapper(WrapperNum).HWLoopSideNum,
                                    Wrapper(WrapperNum).HWBranchNum,
                                    Wrapper(WrapperNum).HWCompNum,
                                    errFlag,
                                    _,
                                    _,
                                    _,
                                    Wrapper(WrapperNum).HWInletNodeNum,
                                    _);

            PlantUtilities::ScanPlantLoopsForObject(Wrapper(WrapperNum).Name,
                                    DataPlant::TypeOf_CentralGroundSourceHeatPump,
                                    Wrapper(WrapperNum).GLHELoopNum,
                                    Wrapper(WrapperNum).GLHELoopSideNum,
                                    Wrapper(WrapperNum).GLHEBranchNum,
                                    Wrapper(WrapperNum).GLHECompNum,
                                    errFlag,
                                    _,
                                    _,
                                    _,
                                    Wrapper(WrapperNum).GLHEInletNodeNum,
                                    _);

            PlantUtilities::InterConnectTwoPlantLoopSides(Wrapper(WrapperNum).CWLoopNum,
                                          Wrapper(WrapperNum).CWLoopSideNum,
                                          Wrapper(WrapperNum).GLHELoopNum,
                                          Wrapper(WrapperNum).GLHELoopSideNum,
                                          DataPlant::TypeOf_CentralGroundSourceHeatPump,
                                          true);

            PlantUtilities::InterConnectTwoPlantLoopSides(Wrapper(WrapperNum).HWLoopNum,
                                          Wrapper(WrapperNum).HWLoopSideNum,
                                          Wrapper(WrapperNum).GLHELoopNum,
                                          Wrapper(WrapperNum).GLHELoopSideNum,
                                          DataPlant::TypeOf_CentralGroundSourceHeatPump,
                                          true);

            PlantUtilities::InterConnectTwoPlantLoopSides(Wrapper(WrapperNum).CWLoopNum,
                                          Wrapper(WrapperNum).CWLoopSideNum,
                                          Wrapper(WrapperNum).HWLoopNum,
                                          Wrapper(WrapperNum).HWLoopSideNum,
                                          DataPlant::TypeOf_CentralGroundSourceHeatPump,
                                          true);

            if (Wrapper(WrapperNum).VariableFlowCH) {
                // Reset flow priority
                if (LoopNum == Wrapper(WrapperNum).CWLoopNum) {
                    DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum)
                        .LoopSide(Wrapper(WrapperNum).CWLoopSideNum)
                        .Branch(Wrapper(WrapperNum).CWBranchNum)
                        .Comp(Wrapper(WrapperNum).CWCompNum)
                        .FlowPriority = DataPlant::LoopFlowStatus_NeedyIfLoopOn;
                } else if (LoopNum == Wrapper(WrapperNum).HWLoopNum) {
                    DataPlant::PlantLoop(Wrapper(WrapperNum).HWLoopNum)
                        .LoopSide(Wrapper(WrapperNum).HWLoopSideNum)
                        .Branch(Wrapper(WrapperNum).HWBranchNum)
                        .Comp(Wrapper(WrapperNum).HWCompNum)
                        .FlowPriority = DataPlant::LoopFlowStatus_NeedyIfLoopOn;
                }

                // check if setpoint on outlet node - chilled water loop
                if (DataLoopNode::Node(Wrapper(WrapperNum).CHWOutletNodeNum).TempSetPoint == DataLoopNode::SensedNodeFlagValue) {
                    if (!DataGlobals::AnyEnergyManagementSystemInModel) {
                        if (!Wrapper(WrapperNum).CoolSetPointErrDone) {
                            ShowWarningError("Missing temperature setpoint on cooling side for CentralHeatPumpSystem named " +
                                             Wrapper(WrapperNum).Name);
                            ShowContinueError(
                                "  A temperature setpoint is needed at the outlet node of a CentralHeatPumpSystem, use a SetpointManager");
                            ShowContinueError("  The overall loop setpoint will be assumed for CentralHeatPumpSystem. The simulation continues ... ");
                            Wrapper(WrapperNum).CoolSetPointErrDone = true;
                        }
                    } else {
                        // need call to EMS to check node
                        bool FatalError = false; // but not really fatal yet, but should be.
                        EMSManager::CheckIfNodeSetPointManagedByEMS(Wrapper(WrapperNum).CHWOutletNodeNum, EMSManager::iTemperatureSetPoint, FatalError);
                        if (FatalError) {
                            if (!Wrapper(WrapperNum).CoolSetPointErrDone) {
                                ShowWarningError("Missing temperature setpoint on cooling side for CentralHeatPumpSystem named " +
                                                 Wrapper(WrapperNum).Name);
                                ShowContinueError("A temperature setpoint is needed at the outlet node of a CentralHeatPumpSystem ");
                                ShowContinueError("use a Setpoint Manager to establish a setpoint at the chiller side outlet node ");
                                ShowContinueError("or use an EMS actuator to establish a setpoint at the outlet node ");
                                ShowContinueError("The overall loop setpoint will be assumed for chiller side. The simulation continues ... ");
                                Wrapper(WrapperNum).CoolSetPointErrDone = true;
                            }
                        }
                    }
                    Wrapper(WrapperNum).CoolSetPointSetToLoop = true;
                    DataLoopNode::Node(Wrapper(WrapperNum).CHWOutletNodeNum).TempSetPoint =
                        DataLoopNode::Node(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).TempSetPointNodeNum).TempSetPoint;
                }

                if (DataLoopNode::Node(Wrapper(WrapperNum).HWOutletNodeNum).TempSetPoint == DataLoopNode::SensedNodeFlagValue) {
                    if (!DataGlobals::AnyEnergyManagementSystemInModel) {
                        if (!Wrapper(WrapperNum).HeatSetPointErrDone) {
                            ShowWarningError("Missing temperature setpoint on heating side for CentralHeatPumpSystem named " +
                                             Wrapper(WrapperNum).Name);
                            ShowContinueError(
                                "  A temperature setpoint is needed at the outlet node of a CentralHeatPumpSystem, use a SetpointManager");
                            ShowContinueError("  The overall loop setpoint will be assumed for CentralHeatPumpSystem. The simulation continues ... ");
                            Wrapper(WrapperNum).HeatSetPointErrDone = true;
                        }
                    } else {
                        // need call to EMS to check node
                        bool FatalError = false; // but not really fatal yet, but should be.
                        EMSManager::CheckIfNodeSetPointManagedByEMS(Wrapper(WrapperNum).HWOutletNodeNum, EMSManager::iTemperatureSetPoint, FatalError);
                        if (FatalError) {
                            if (!Wrapper(WrapperNum).HeatSetPointErrDone) {
                                ShowWarningError("Missing temperature setpoint on heating side for CentralHeatPumpSystem named " +
                                                 Wrapper(WrapperNum).Name);
                                ShowContinueError("A temperature setpoint is needed at the outlet node of a CentralHeatPumpSystem ");
                                ShowContinueError("use a Setpoint Manager to establish a setpoint at the chiller side outlet node ");
                                ShowContinueError("or use an EMS actuator to establish a setpoint at the outlet node ");
                                ShowContinueError("The overall loop setpoint will be assumed for chiller side. The simulation continues ... ");
                                Wrapper(WrapperNum).HeatSetPointErrDone = true;
                            }
                        }
                    }
                    Wrapper(WrapperNum).HeatSetPointSetToLoop = true;
                    DataLoopNode::Node(Wrapper(WrapperNum).HWOutletNodeNum).TempSetPoint =
                        DataLoopNode::Node(DataPlant::PlantLoop(Wrapper(WrapperNum).HWLoopNum).TempSetPointNodeNum).TempSetPoint;
                }
            }
            Wrapper(WrapperNum).MyWrapperFlag = false;
        }

        if (Wrapper(WrapperNum).MyWrapperEnvrnFlag && DataGlobals::BeginEnvrnFlag && (DataPlant::PlantFirstSizesOkayToFinalize)) {

            if (Wrapper(WrapperNum).ControlMode == SmartMixing) {

                Wrapper(WrapperNum).CHWVolFlowRate = 0.0;
                Wrapper(WrapperNum).HWVolFlowRate = 0.0;
                Wrapper(WrapperNum).GLHEVolFlowRate = 0.0;

                for (int ChillerHeaterNum = 1; ChillerHeaterNum <= Wrapper(WrapperNum).ChillerHeaterNums; ++ChillerHeaterNum) {
                    Wrapper(WrapperNum).CHWVolFlowRate += Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapVolFlowRate;
                    Wrapper(WrapperNum).HWVolFlowRate += Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).DesignHotWaterVolFlowRate;
                    Wrapper(WrapperNum).GLHEVolFlowRate += Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondVolFlowRate;
                }

                Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidIndex,
                                       RoutineName);

                Wrapper(WrapperNum).CHWMassFlowRateMax = Wrapper(WrapperNum).CHWVolFlowRate * rho;
                Wrapper(WrapperNum).HWMassFlowRateMax = Wrapper(WrapperNum).HWVolFlowRate * rho;
                Wrapper(WrapperNum).GLHEMassFlowRateMax = Wrapper(WrapperNum).GLHEVolFlowRate * rho;

                PlantUtilities::InitComponentNodes(0.0,
                                   Wrapper(WrapperNum).CHWMassFlowRateMax,
                                                   Wrapper(WrapperNum).CHWInletNodeNum,
                                                   Wrapper(WrapperNum).CHWOutletNodeNum,
                                   Wrapper(WrapperNum).CWLoopNum,
                                   Wrapper(WrapperNum).CWLoopSideNum,
                                   Wrapper(WrapperNum).CWBranchNum,
                                   Wrapper(WrapperNum).CWCompNum);
                PlantUtilities::InitComponentNodes(0.0,
                                   Wrapper(WrapperNum).HWMassFlowRateMax,
                                                   Wrapper(WrapperNum).HWInletNodeNum,
                                                   Wrapper(WrapperNum).HWOutletNodeNum,
                                   Wrapper(WrapperNum).HWLoopNum,
                                   Wrapper(WrapperNum).HWLoopSideNum,
                                   Wrapper(WrapperNum).HWBranchNum,
                                   Wrapper(WrapperNum).HWCompNum);
                PlantUtilities::InitComponentNodes(0.0,
                                   Wrapper(WrapperNum).GLHEMassFlowRateMax,
                                                   Wrapper(WrapperNum).GLHEInletNodeNum,
                                                   Wrapper(WrapperNum).GLHEOutletNodeNum,
                                   Wrapper(WrapperNum).GLHELoopNum,
                                   Wrapper(WrapperNum).GLHELoopSideNum,
                                   Wrapper(WrapperNum).GLHEBranchNum,
                                   Wrapper(WrapperNum).GLHECompNum);

                // Initialize nodes for individual chiller heaters
                for (int ChillerHeaterNum = 1; ChillerHeaterNum <= Wrapper(WrapperNum).ChillerHeaterNums; ++ChillerHeaterNum) {
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.MassFlowRateMin = 0.0;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.MassFlowRateMinAvail = 0.0;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.MassFlowRateMax =
                        rho * Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapVolFlowRate;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.MassFlowRateMaxAvail =
                        rho * Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapVolFlowRate;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.MassFlowRate = 0.0;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.MassFlowRateMin = 0.0;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.MassFlowRateMinAvail = 0.0;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.MassFlowRateMax =
                        rho * Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapVolFlowRate;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.MassFlowRateMaxAvail =
                        rho * Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapVolFlowRate;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.MassFlowRate = 0.0;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.MassFlowRateRequest = 0.0;
                }
            }
            Wrapper(WrapperNum).MyWrapperEnvrnFlag = false;
        }

        if (!DataGlobals::BeginEnvrnFlag) {
            Wrapper(WrapperNum).MyWrapperEnvrnFlag = true;
        }

        if (Wrapper(WrapperNum).CoolSetPointSetToLoop) {
            // IF (CurCoolingLoad > 0.0d0) THEN
            DataLoopNode::Node(Wrapper(WrapperNum).CHWOutletNodeNum).TempSetPoint = DataLoopNode::Node(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).TempSetPointNodeNum).TempSetPoint;
        }
        // IF (CurHeatingLoad > 0.0d0) THEN
        if (Wrapper(WrapperNum).HeatSetPointSetToLoop) {
            DataLoopNode::Node(Wrapper(WrapperNum).HWOutletNodeNum).TempSetPoint = DataLoopNode::Node(DataPlant::PlantLoop(Wrapper(WrapperNum).HWLoopNum).TempSetPointNodeNum).TempSetPoint;
            // ENDIF
        }

        Real64 mdotCHW;                         // Chilled water mass flow rate
        Real64 mdotHW;                          // Hot water mass flow rate
        Real64 mdotGLHE;                        // Condenser water mass flow rate

        // Switch over the mass flow rate to the condenser loop, i.e., ground heat exchanger
        if (LoopNum == Wrapper(WrapperNum).CWLoopNum) { // called for on cooling loop
            if (MyLoad < -1.0) {                        // calling for cooling
                mdotCHW = DataLoopNode::Node(Wrapper(WrapperNum).CHWInletNodeNum).MassFlowRateMax;
            } else {
                mdotCHW = 0.0;
            }
            if (Wrapper(WrapperNum).WrapperHeatingLoad > 1.0) {
                mdotHW = DataLoopNode::Node(Wrapper(WrapperNum).HWInletNodeNum).MassFlowRateMax;
            } else {
                mdotHW = 0.0;
            }
            if ((MyLoad < -1.0) || (Wrapper(WrapperNum).WrapperHeatingLoad > 1.0)) {
                mdotGLHE = DataLoopNode::Node(Wrapper(WrapperNum).GLHEInletNodeNum).MassFlowRateMax;
            } else {
                mdotGLHE = 0.0;
            }

        } else if (LoopNum == Wrapper(WrapperNum).HWLoopNum) {
            if (MyLoad > 1.0) {
                mdotHW = DataLoopNode::Node(Wrapper(WrapperNum).HWInletNodeNum).MassFlowRateMax;
            } else {
                mdotHW = 0.0;
            }
            if (Wrapper(WrapperNum).WrapperCoolingLoad > 1.0) {
                mdotCHW = DataLoopNode::Node(Wrapper(WrapperNum).CHWInletNodeNum).MassFlowRateMax;
            } else {
                mdotCHW = 0.0;
            }
            if ((MyLoad > 1.0) || (Wrapper(WrapperNum).WrapperCoolingLoad > 1.0)) {
                mdotGLHE = DataLoopNode::Node(Wrapper(WrapperNum).GLHEInletNodeNum).MassFlowRateMax;
            } else {
                mdotGLHE = 0.0;
            }

        } else if (LoopNum == Wrapper(WrapperNum).GLHELoopNum) {
            if (Wrapper(WrapperNum).WrapperCoolingLoad > 1.0) {
                mdotCHW = DataLoopNode::Node(Wrapper(WrapperNum).CHWInletNodeNum).MassFlowRateMax;
            } else {
                mdotCHW = 0.0;
            }
            if (Wrapper(WrapperNum).WrapperHeatingLoad > 1.0) {
                mdotHW = DataLoopNode::Node(Wrapper(WrapperNum).HWInletNodeNum).MassFlowRateMax;
            } else {
                mdotHW = 0.0;
            }
            if ((Wrapper(WrapperNum).WrapperHeatingLoad > 1.0) || (Wrapper(WrapperNum).WrapperCoolingLoad > 1.0)) {
                mdotGLHE = DataLoopNode::Node(Wrapper(WrapperNum).GLHEInletNodeNum).MassFlowRateMax;
            } else {
                mdotGLHE = 0.0;
            }
        }

        PlantUtilities::SetComponentFlowRate(mdotCHW,
                                             Wrapper(WrapperNum).CHWInletNodeNum,
                                             Wrapper(WrapperNum).CHWOutletNodeNum,
                             Wrapper(WrapperNum).CWLoopNum,
                             Wrapper(WrapperNum).CWLoopSideNum,
                             Wrapper(WrapperNum).CWBranchNum,
                             Wrapper(WrapperNum).CWCompNum);

        PlantUtilities::SetComponentFlowRate(mdotHW,
                                             Wrapper(WrapperNum).HWInletNodeNum,
                                             Wrapper(WrapperNum).HWOutletNodeNum,
                             Wrapper(WrapperNum).HWLoopNum,
                             Wrapper(WrapperNum).HWLoopSideNum,
                             Wrapper(WrapperNum).HWBranchNum,
                             Wrapper(WrapperNum).HWCompNum);

        PlantUtilities::SetComponentFlowRate(mdotGLHE,
                                             Wrapper(WrapperNum).GLHEInletNodeNum,
                                             Wrapper(WrapperNum).GLHEOutletNodeNum,
                             Wrapper(WrapperNum).GLHELoopNum,
                             Wrapper(WrapperNum).GLHELoopSideNum,
                             Wrapper(WrapperNum).GLHEBranchNum,
                             Wrapper(WrapperNum).GLHECompNum);
    }

    void CalcChillerModel(int const WrapperNum,                 // Number of wrapper
                          int const EP_UNUSED(OpMode),          // Operation mode
                          Real64 &EP_UNUSED(MyLoad),            // Operating load
                          bool const EP_UNUSED(RunFlag),        // TRUE when chiller operating
                          bool const EP_UNUSED(FirstIteration), // TRUE when first iteration of timestep
                          int const EP_UNUSED(EquipFlowCtrl),   // Flow control mode for the equipment
                          int const EP_UNUSED(LoopNum)          // Plant loop number
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Daeho Kang, PNNL
        //       DATE WRITTEN   Feb 2013
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        //  Simulate a ChillerHeaterPerformance:Electric:EIR using curve fit

        // METHODOLOGY EMPLOYED:
        //  Use empirical curve fits to model performance at off-reference conditions

        // REFERENCES:
        // 1. DOE-2 Engineers Manual, Version 2.1A, November 1982, LBL-11353

        static std::string const RoutineName("CalcChillerHeaterModel");
        static std::string const RoutineNameElecEIRChiller("CalcElectricEIRChillerModel");

        bool IsLoadCoolRemaining(true);
        bool NextCompIndicator(false); // Component indicator when identical chiller heaters exist
        int CompNum = 0;                       // Component number in the loop  REAL(r64) :: FRAC
        int IdenticalUnitCounter = 0;             // Pointer to count number of identical unit passed
        Real64 CurAvailCHWMassFlowRate(0.0);  // Maximum available mass flow rate for current chiller heater

        // Cooling load evaporator should meet
        Real64 EvaporatorLoad = Wrapper(WrapperNum).WrapperCoolingLoad;

        // Chilled water inlet mass flow rate
        Real64 CHWInletMassFlowRate = DataLoopNode::Node(Wrapper(WrapperNum).CHWInletNodeNum).MassFlowRate;

        for (int ChillerHeaterNum = 1; ChillerHeaterNum <= Wrapper(WrapperNum).ChillerHeaterNums; ++ChillerHeaterNum) {

            // Initialize local variables for each chiller heater
            int CurrentMode = 0;
            ChillerCapFT = 0.0;
            ChillerEIRFT = 0.0;
            ChillerEIRFPLR = 0.0;
            ChillerPartLoadRatio = 0.0;
            ChillerCyclingRatio = 0.0;
            ChillerFalseLoadRate = 0.0;

            Real64 CHPower = 0.0;
            Real64 QCondenser = 0.0;
            Real64 QEvaporator = 0.0;
            Real64 FRAC = 1.0;
            Real64 ActualCOP = 0.0;
            Real64 EvapInletTemp = DataLoopNode::Node(Wrapper(WrapperNum).CHWInletNodeNum).Temp;
            Real64 CondInletTemp = DataLoopNode::Node(Wrapper(WrapperNum).GLHEInletNodeNum).Temp;
            Real64 EvapOutletTemp = EvapInletTemp;
            Real64 CondOutletTemp = CondInletTemp;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CurrentMode = 0;

            // Find proper schedule values
            if (Wrapper(WrapperNum).NumOfComp != Wrapper(WrapperNum).ChillerHeaterNums) { // Identical units exist
                if (ChillerHeaterNum == 1) {
                    IdenticalUnitCounter = 0;
                    NextCompIndicator = false;
                    CompNum = ChillerHeaterNum;
                }
                if (NextCompIndicator) {
                    ++CompNum;
                }
                if (CompNum == 1) {
                    if (ChillerHeaterNum != Wrapper(WrapperNum).WrapperComp(CompNum).WrapperIdenticalObjectNum) {
                        NextCompIndicator = false;
                    } else if (ChillerHeaterNum == Wrapper(WrapperNum).WrapperComp(CompNum).WrapperIdenticalObjectNum) {
                        NextCompIndicator = true;
                    }
                } else if (CompNum > 1) {
                    if ((ChillerHeaterNum - ((ChillerHeaterNum - 1) - IdenticalUnitCounter)) !=
                        Wrapper(WrapperNum).WrapperComp(CompNum).WrapperIdenticalObjectNum) {
                        NextCompIndicator = false;
                    } else if ((ChillerHeaterNum - ((ChillerHeaterNum - 1) - IdenticalUnitCounter)) ==
                               Wrapper(WrapperNum).WrapperComp(CompNum).WrapperIdenticalObjectNum) {
                        NextCompIndicator = true;
                    }
                }
                ++IdenticalUnitCounter;
                int IdenticalUnitRemaining = Wrapper(WrapperNum).WrapperComp(CompNum).WrapperIdenticalObjectNum - IdenticalUnitCounter;
                if (IdenticalUnitRemaining == 0) IdenticalUnitCounter = 0;
            } else if (Wrapper(WrapperNum).NumOfComp == Wrapper(WrapperNum).ChillerHeaterNums) {
                ++CompNum;
            }

            if (CompNum > Wrapper(WrapperNum).NumOfComp) {
                ShowSevereError("CalcChillerModel: ChillerHeater=\"" + Wrapper(WrapperNum).Name + "\", calculated component number too big.");
                ShowContinueError("Max number of components=[" + General::RoundSigDigits(Wrapper(WrapperNum).NumOfComp) + "], indicated component number=[" +
                                  General::RoundSigDigits(CompNum) + "].");
                ShowFatalError("Program terminates due to preceding condition.");
            }

            Real64 EvapMassFlowRate;              // Actual evaporator mass flow rate
            Real64 CondMassFlowRate;              // Condenser mass flow rate

            // Check whether this chiller heater needs to run
            if (EvaporatorLoad > 0.0 && (ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).WrapperComp(CompNum).CHSchedPtr) > 0.0)) {
                IsLoadCoolRemaining = true;

                // Calculate density ratios to adjust mass flow rates from initialized ones
                // Hot water temperature is known, but evaporator mass flow rates will be adjusted in the following "Do" loop
                Real64 InitDensity = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidName,
                                               DataGlobals::CWInitConvTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidIndex,
                                               RoutineName);
                Real64 EvapDensity = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidName,
                                               EvapInletTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidIndex,
                                               RoutineName);
                Real64 CondDensity = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidName,
                                               CondInletTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidIndex,
                                               RoutineName);

                // Calculate density ratios to adjust mass flow rates from initialized ones

                // Fraction between standardized density and local density in the chilled water side
                Real64 CHWDensityRatio = EvapDensity / InitDensity;

                // Fraction between standardized density and local density in the condenser side
                Real64 GLHEDensityRatio = CondDensity / InitDensity;
                CondMassFlowRate = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.MassFlowRateMaxAvail;
                EvapMassFlowRate = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.MassFlowRateMaxAvail;
                EvapMassFlowRate *= CHWDensityRatio;
                CondMassFlowRate *= GLHEDensityRatio;

                // Check available flows from plant and then adjust as necessary
                if (CurAvailCHWMassFlowRate == 0) { // The very first chiller heater to operate
                    CurAvailCHWMassFlowRate = CHWInletMassFlowRate;
                } else if (ChillerHeaterNum > 1) {
                    CurAvailCHWMassFlowRate -= Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum - 1).EvapOutletNode.MassFlowRate;
                }
                EvapMassFlowRate = min(CurAvailCHWMassFlowRate, EvapMassFlowRate);
            } else {
                IsLoadCoolRemaining = false;
                EvapMassFlowRate = 0.0;
                CondMassFlowRate = 0.0;
                CurrentMode = 0;
            }

            // Chiller heater is on when cooling load for this chiller heater remains and chilled water available
            if (IsLoadCoolRemaining && (EvapMassFlowRate > 0) && (ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).WrapperComp(CompNum).CHSchedPtr) > 0)) {
                // Indicate current mode is cooling-only mode. Simultaneous clg/htg mode will be set later
                CurrentMode = 1;

                // Assign proper performance curve information depending on the control mode
                // Cooling curve is used only for cooling-only mode, and the others (Simultaneous and heating) read the heating curve
                if (Wrapper(WrapperNum).SimulClgDominant || Wrapper(WrapperNum).SimulHtgDominant) {
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCap = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCapClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCOP = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCOPClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefEvapOut =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefEvapOutClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondIn =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondInClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondOut =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondOutClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).OptPartLoadRat =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).OptPartLoadRatClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondMode =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondModeHeating;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTIDX =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTHeatingIDX;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFTIDX =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFTHeatingIDX;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRIDX =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRHeatingIDX;
                } else {
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCap = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCapCooling;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCOP = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCOPCooling;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefEvapOut =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefEvapOutCooling;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondIn =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondInCooling;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondOut =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondOutCooling;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).OptPartLoadRat =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).OptPartLoadRatCooling;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondMode =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondModeCooling;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTIDX =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTCoolingIDX;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFTIDX =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFTCoolingIDX;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRIDX =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRCoolingIDX;
                }

                // Only used to read curve values
                CondOutletTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondOutCooling;
                Real64 CondTempforCurve;
                if (Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondMode == "ENTERINGCONDENSER") {
                    CondTempforCurve = CondInletTemp;
                } else if (Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondMode == "LEAVINGCONDENSER") {
                    CondTempforCurve = CondOutletTemp;
                } else {
                    ShowWarningError("ChillerHeaterPerformance:Electric:EIR \"" + Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name + "\":");
                    ShowContinueError("Chiller condenser temperature for curve fit are not decided, defalt value= cond_leaving (" +
                                      General::RoundSigDigits(ChillerCapFT, 3) + ").");
                    CondTempforCurve = CondOutletTemp;
                }

                // Bind local variables from the curve
                Real64 MinPartLoadRat;                // Min allowed operating fraction of full load
                Real64 MaxPartLoadRat;                // Max allowed operating fraction of full load

                CurveManager::GetCurveMinMaxValues(Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRIDX, MinPartLoadRat, MaxPartLoadRat);

                // Chiller reference capacity
                Real64 ChillerRefCap = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCap;
                Real64 ReferenceCOP = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCOP;
                Real64 TempLowLimitEout = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempLowLimitEvapOut;
                Real64 EvapOutletTempSetPoint = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefEvapOutCooling;
                ChillerCapFT = CurveManager::CurveValue(Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTIDX, EvapOutletTempSetPoint, CondTempforCurve);

                if (ChillerCapFT < 0) {
                    if (Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTError < 1 && !DataGlobals::WarmupFlag) {
                        ++Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTError;
                        ShowWarningError("ChillerHeaterPerformance:Electric:EIR \"" + Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name +
                                         "\":");
                        ShowContinueError(" ChillerHeater Capacity as a Function of Temperature curve output is negative (" +
                                          General::RoundSigDigits(ChillerCapFT, 3) + ").");
                        ShowContinueError(" Negative value occurs using an Evaporator Outlet Temp of " + General::RoundSigDigits(EvapOutletTempSetPoint, 1) +
                                          " and a Condenser Inlet Temp of " + General::RoundSigDigits(CondInletTemp, 1) + '.');
                        ShowContinueErrorTimeStamp(" Resetting curve output to zero and continuing simulation.");
                    } else if (!DataGlobals::WarmupFlag) {
                        ++Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTError;
                        ShowRecurringWarningErrorAtEnd(
                            "ChillerHeaterPerformance:Electric:EIR \"" + Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name +
                                "\": ChillerHeater Capacity as a Function of Temperature curve output is negative warning continues...",
                            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTErrorIndex,
                            ChillerCapFT,
                            ChillerCapFT);
                    }
                    ChillerCapFT = 0.0;
                }

                // Calculate the specific heat of chilled water
                Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidName,
                                           EvapInletTemp,
                                           DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidIndex,
                                           RoutineName);

                // Calculate cooling load this chiller should meet and the other chillers are demanded
                EvapOutletTempSetPoint = DataLoopNode::Node(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).TempSetPointNodeNum).TempSetPoint;

                // Minimum capacity of the evaporator
                Real64 EvaporatorCapMin = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).MinPartLoadRatCooling *
                                   Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCapCooling;

                // Remaining cooling load the other chiller heaters should meet
                Real64 CoolingLoadToMeet =
                    min(Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCapCooling, max(std::abs(EvaporatorLoad), EvaporatorCapMin));

                // Available chiller capacity as a function of temperature
                // Chiller available capacity at current operating conditions [W]
                Real64 AvailChillerCap = ChillerRefCap * ChillerCapFT;

                // Set load this chiller heater should meet
                QEvaporator = min(CoolingLoadToMeet, (AvailChillerCap * MaxPartLoadRat));
                EvapOutletTemp = EvapOutletTempSetPoint;
                Real64 EvapDeltaTemp = EvapInletTemp - EvapOutletTemp;

                Real64 PartLoadRat;                   // Operating part load ratio

                // Calculate temperatures for constant flow and mass flow rates for variable flow
                if (EvapMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    if (Wrapper(WrapperNum).SimulHtgDominant) { // Evaporator operates at full capacity for heating
                        PartLoadRat = max(0.0, min((ChillerRefCap / AvailChillerCap), MaxPartLoadRat));
                        QEvaporator = AvailChillerCap * PartLoadRat;
                        EvapDeltaTemp = QEvaporator / EvapMassFlowRate / Cp;
                        EvapOutletTemp = EvapInletTemp - EvapDeltaTemp;
                    } else {                                      // Cooling only mode or cooling dominant simultaneous htg/clg mode
                        if (Wrapper(WrapperNum).VariableFlowCH) { // Variable flow
                            Real64 EvapMassFlowRateCalc = QEvaporator / EvapDeltaTemp / Cp;
                            if (EvapMassFlowRateCalc > EvapMassFlowRate) {
                                EvapMassFlowRateCalc = EvapMassFlowRate;
                                Real64 EvapDeltaTempCalc = QEvaporator / EvapMassFlowRate / Cp;
                                EvapOutletTemp = EvapInletTemp - EvapDeltaTempCalc;
                                if (EvapDeltaTempCalc > EvapDeltaTemp) {
                                    QEvaporator = EvapMassFlowRate * Cp * EvapDeltaTemp;
                                }
                            }
                            EvapMassFlowRate = EvapMassFlowRateCalc;
                        } else { // Constant Flow
                            Real64 EvapOutletTempCalc = EvapInletTemp - EvapDeltaTemp;
                            if (EvapOutletTempCalc > EvapOutletTemp) { // Load to meet should be adjusted
                                EvapOutletTempCalc = EvapOutletTemp;
                                QEvaporator = EvapMassFlowRate * Cp * EvapDeltaTemp;
                            }
                            EvapOutletTemp = EvapOutletTempCalc;
                        } // End of flow control decision
                    }     // End of operation mode
                } else {
                    QEvaporator = 0.0;
                    EvapOutletTemp = EvapInletTemp;
                }

                // Check evaporator temperature low limit and adjust capacity if needed
                if (EvapOutletTemp < TempLowLimitEout) {
                    if ((EvapInletTemp - TempLowLimitEout) > DataPlant::DeltaTempTol) {
                        EvapOutletTemp = TempLowLimitEout;
                        EvapDeltaTemp = EvapInletTemp - EvapOutletTemp;
                        QEvaporator = EvapMassFlowRate * Cp * EvapDeltaTemp;
                    } else {
                        QEvaporator = 0.0;
                        EvapOutletTemp = EvapInletTemp;
                    }
                }

                // Check if the outlet temperature exceeds the node minimum temperature and adjust capacity if needed
                if (EvapOutletTemp < Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.TempMin) {
                    if ((Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp -
                         Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.TempMin) > DataPlant::DeltaTempTol) {
                        EvapOutletTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.TempMin;
                        EvapDeltaTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.TempMin - EvapOutletTemp;
                        QEvaporator = EvapMassFlowRate * Cp * EvapDeltaTemp;
                    } else {
                        QEvaporator = 0.0;
                        EvapOutletTemp = EvapInletTemp;
                    }
                }

                // Calculate part load once more since evaporator capacity might be modified
                if (AvailChillerCap > 0.0) {
                    PartLoadRat = max(0.0, min((QEvaporator / AvailChillerCap), MaxPartLoadRat));
                } else {
                    PartLoadRat = 0.0;
                }

                // Chiller cycles below minimum part load ratio, FRAC = amount of time chiller is ON during this time step
                if (PartLoadRat < MinPartLoadRat) FRAC = min(1.0, (PartLoadRat / MinPartLoadRat));

                // set the module level variable used for reporting FRAC
                ChillerCyclingRatio = FRAC;

                // Chiller is false loading below PLR = minimum unloading ratio, find PLR used for energy calculation
                if (AvailChillerCap > 0.0) {
                    PartLoadRat = max(PartLoadRat, MinPartLoadRat);
                } else {
                    PartLoadRat = 0.0;
                }

                // set the module level variable used for reporting PLR
                ChillerPartLoadRatio = PartLoadRat;

                // calculate the load due to false loading on chiller over and above water side load
                ChillerFalseLoadRate = (AvailChillerCap * PartLoadRat * FRAC) - QEvaporator;
                if (ChillerFalseLoadRate < DataHVACGlobals::SmallLoad) {
                    ChillerFalseLoadRate = 0.0;
                }

                // Determine chiller compressor power and transfer heat calculation
                ChillerEIRFT =
                    max(0.0, CurveManager::CurveValue(Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFTIDX, EvapOutletTemp, CondTempforCurve));
                ChillerEIRFPLR = max(0.0, CurveManager::CurveValue(Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRIDX, PartLoadRat));
                CHPower = (AvailChillerCap / ReferenceCOP) * ChillerEIRFPLR * ChillerEIRFT * FRAC;
                QCondenser = CHPower * Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).OpenMotorEff + QEvaporator + ChillerFalseLoadRate;
                ActualCOP = (QEvaporator + ChillerFalseLoadRate) / CHPower;

                if (CondMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).GLHELoopNum).FluidName,
                                               CondInletTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).GLHELoopNum).FluidIndex,
                                               RoutineNameElecEIRChiller);
                    CondOutletTemp = QCondenser / CondMassFlowRate / Cp + CondInletTemp;
                } else {
                    ShowSevereError("CalcChillerheaterModel: Condenser flow = 0, for Chillerheater=" +
                                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name);
                    ShowContinueErrorTimeStamp("");
                }

                // Determine load next chillers should meet
                if (EvaporatorLoad < QEvaporator) {
                    EvaporatorLoad = 0.0; // No remaining load so the rest will be off
                } else {
                    EvaporatorLoad -= QEvaporator;
                }

                // Initialize reporting variable when this chiller doesn't need to operate
                if (QEvaporator == 0.0) {
                    CurrentMode = 0;
                    ChillerPartLoadRatio = 0.0;
                    ChillerCyclingRatio = 0.0;
                    ChillerFalseLoadRate = 0.0;
                    EvapMassFlowRate = 0.0;
                    CondMassFlowRate = 0.0;
                    CHPower = 0.0;
                    QCondenser = 0.0;
                    EvapOutletTemp = EvapInletTemp;
                    CondOutletTemp = CondInletTemp;
                    EvaporatorLoad = 0.0;
                }

            } // End of calculation for cooling

            // Set variables to the arrays
            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.MassFlowRate = EvapMassFlowRate;
            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondOutletNode.MassFlowRate = CondMassFlowRate;
            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.Temp = EvapOutletTemp;
            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp = EvapInletTemp;
            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondOutletNode.Temp = CondOutletTemp;
            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.Temp = CondInletTemp;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CurrentMode = CurrentMode;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerPartLoadRatio = ChillerPartLoadRatio;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCyclingRatio = ChillerCyclingRatio;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoadRate = ChillerFalseLoadRate;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCapFT = ChillerCapFT;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFT = ChillerEIRFT;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFPLR = ChillerEIRFPLR;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPower = CHPower;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingPower = 0.0;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvap = QEvaporator;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCond = QCondenser;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTemp = EvapOutletTemp;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapInletTemp = EvapInletTemp;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTemp = CondOutletTemp;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondInletTemp = CondInletTemp;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Evapmdot = EvapMassFlowRate;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot = CondMassFlowRate;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ActualCOP = ActualCOP;

            if (Wrapper(WrapperNum).SimulClgDominant || Wrapper(WrapperNum).SimulHtgDominant) { // Store for using these cooling side data in the hot water loop
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CurrentMode = CurrentMode;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerPartLoadRatioSimul = ChillerPartLoadRatio;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCyclingRatioSimul = ChillerCyclingRatio;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoadRateSimul = ChillerFalseLoadRate;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCapFTSimul = ChillerCapFT;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFTSimul = ChillerEIRFT;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFPLRSimul = ChillerEIRFPLR;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPowerSimul = CHPower;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvapSimul = QEvaporator;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTempSimul = EvapOutletTemp;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapInletTempSimul = EvapInletTemp;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapmdotSimul = EvapMassFlowRate;
                if (Wrapper(WrapperNum).SimulClgDominant) {
                    Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCondSimul = QCondenser;
                    Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTempSimul = CondOutletTemp;
                    Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondInletTempSimul = CondInletTemp;
                    Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondmdotSimul = CondMassFlowRate;
                }
            }
        }
    }

    void CalcChillerHeaterModel(int const WrapperNum,                 // Wrapper number pointor
                                int const EP_UNUSED(OpMode),          // Operation mode
                                Real64 &EP_UNUSED(MyLoad),            // Heating load plant should meet
                                bool const EP_UNUSED(RunFlag),        // TRUE when chiller operating
                                bool const EP_UNUSED(FirstIteration), // TRUE when first iteration of timestep
                                int const EP_UNUSED(EquipFlowCtrl),   // Flow control mode for the equipment
                                int const EP_UNUSED(LoopNum)          // Loop number
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Daeho Kang, PNNL
        //       DATE WRITTEN   Feb 2013
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        //  Simulate a ChillerHeaterPerformance:Electric:EIR using curve fit

        // METHODOLOGY EMPLOYED:
        //  Use empirical curve fits to model performance at off-reference conditions

        // REFERENCES:
        // 1. DOE-2 Engineers Manual, Version 2.1A, November 1982, LBL-11353

        static std::string const RoutineName("CalcChillerHeaterModel");
        static std::string const RoutineNameElecEIRChiller("CalcElectricEIRChillerModel");

        bool IsLoadHeatRemaining(true); // Ture if heating load remains for this chiller heater
        bool NextCompIndicator(false);  // Component indicator when identical chiller heaters exist
        int CompNum(0);                        // Component number
        int IdenticalUnitCounter = 0;              // Pointer to count number of identical unit passed
        int IdenticalUnitRemaining;            // Pointer to count number of identical unit available for a component
        Real64 CondenserLoad(0.0);             // Remaining heating load that this wrapper should meet
        Real64 CurAvailHWMassFlowRate(0.0);    // Maximum available hot water mass within the wrapper bank

        CondenserLoad = Wrapper(WrapperNum).WrapperHeatingLoad;
        Real64 HWInletMassFlowRate = DataLoopNode::Node(Wrapper(WrapperNum).HWInletNodeNum).MassFlowRate;

        // Flow
        for (int ChillerHeaterNum = 1; ChillerHeaterNum <= Wrapper(WrapperNum).ChillerHeaterNums; ++ChillerHeaterNum) {

            // Set module level inlet and outlet nodes and initialize other local variables
            int CurrentMode = 0;
            ChillerPartLoadRatio = 0.0;
            ChillerCyclingRatio = 0.0;
            ChillerFalseLoadRate = 0.0;
            Real64 CHPower = 0.0;
            Real64 QCondenser = 0.0;
            Real64 QEvaporator = 0.0;
            Real64 FRAC = 1.0;
            Real64 CondDeltaTemp = 0.0;
            Real64 CoolingPower = 0.0;
            Real64 ActualCOP = 0.0;
            Real64 EvapInletTemp = DataLoopNode::Node(Wrapper(WrapperNum).GLHEInletNodeNum).Temp;
            Real64 CondInletTemp = DataLoopNode::Node(Wrapper(WrapperNum).HWInletNodeNum).Temp;
            Real64 EvapOutletTemp = EvapInletTemp;
            Real64 CondOutletTemp = CondInletTemp;

            // Find proper schedule values
            if (Wrapper(WrapperNum).NumOfComp != Wrapper(WrapperNum).ChillerHeaterNums) { // Identical units exist
                if (ChillerHeaterNum == 1) {
                    IdenticalUnitCounter = 0;
                    NextCompIndicator = false;
                    CompNum = ChillerHeaterNum;
                }
                if (NextCompIndicator) {
                    ++CompNum;
                }
                if (CompNum == 1) {
                    if (ChillerHeaterNum != Wrapper(WrapperNum).WrapperComp(CompNum).WrapperIdenticalObjectNum) {
                        NextCompIndicator = false;
                    } else if (ChillerHeaterNum == Wrapper(WrapperNum).WrapperComp(CompNum).WrapperIdenticalObjectNum) {
                        NextCompIndicator = true;
                    }
                } else if (CompNum > 1) {
                    if ((ChillerHeaterNum - ((ChillerHeaterNum - 1) - IdenticalUnitCounter)) !=
                        Wrapper(WrapperNum).WrapperComp(CompNum).WrapperIdenticalObjectNum) {
                        NextCompIndicator = false;
                    } else if ((ChillerHeaterNum - ((ChillerHeaterNum - 1) - IdenticalUnitCounter)) ==
                               Wrapper(WrapperNum).WrapperComp(CompNum).WrapperIdenticalObjectNum) {
                        NextCompIndicator = true;
                    }
                }
                ++IdenticalUnitCounter;
                IdenticalUnitRemaining = Wrapper(WrapperNum).WrapperComp(CompNum).WrapperIdenticalObjectNum - IdenticalUnitCounter;
                if (IdenticalUnitRemaining == 0) IdenticalUnitCounter = 0;
            } else if (Wrapper(WrapperNum).NumOfComp == Wrapper(WrapperNum).ChillerHeaterNums) {
                ++CompNum;
            }

            Real64 CondMassFlowRate;               // Condenser mass flow rate through this chiller heater
            Real64 EvapMassFlowRate;               // Evaporator mass flow rate through this chiller heater

            // Check to see if this chiller heater needs to run
            if (CondenserLoad > 0.0 && (ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).WrapperComp(CompNum).CHSchedPtr) > 0)) {
                IsLoadHeatRemaining = true;

                // Calculate density ratios to adjust mass flow rates from initialized ones
                // Hot water temperature is known, but condenser mass flow rates will be adjusted in the following "Do" loop
                Real64 InitDensity = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidName,
                                               DataGlobals::CWInitConvTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidIndex,
                                               RoutineName);
                Real64 EvapDensity = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidName,
                                               EvapInletTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidIndex,
                                               RoutineName);
                Real64 CondDensity = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidName,
                                               CondInletTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).CWLoopNum).FluidIndex,
                                               RoutineName);

                // Calculate density ratios to adjust mass flow rates from initialized ones
                Real64 HWDensityRatio = CondDensity / InitDensity;
                Real64 GLHEDensityRatio = EvapDensity / InitDensity;
                EvapMassFlowRate = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.MassFlowRateMaxAvail;
                CondMassFlowRate = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.MassFlowRateMaxAvail;
                EvapMassFlowRate *= GLHEDensityRatio;
                CondMassFlowRate *= HWDensityRatio;

                // Check flows from plant to adjust as necessary
                if (CurAvailHWMassFlowRate == 0) { // First chiller heater which is on
                    CurAvailHWMassFlowRate = HWInletMassFlowRate;
                } else if (ChillerHeaterNum > 1) {
                    CurAvailHWMassFlowRate -= Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum - 1).CondOutletNode.MassFlowRate;
                }
                CondMassFlowRate = min(CurAvailHWMassFlowRate, CondMassFlowRate);

                // It is not enforced to be the smaller of CH max temperature and plant temp setpoint.
                // Hot water temperatures at the individual CHs' outlet may be greater than plant setpoint temp,
                // but should be lower than the CHs max temp
                CondOutletTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondOutClgHtg;
                CondDeltaTemp = CondOutletTemp - CondInletTemp;

                if (CondDeltaTemp < 0.0) { // Hot water temperature is greater than the maximum
                    if (Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRRefTempErrorIndex == 0) {
                        ShowSevereMessage("CalcChillerHeaterModel: ChillerHeaterPerformance:Electric:EIR=\"" +
                                          Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name + "\", DeltaTemp < 0");
                        ShowContinueError(" Reference Simultaneous Cooling-Heating Mode Leaving Condenser Water Temperature [" +
                                          General::RoundSigDigits(CondOutletTemp, 1) + ']');
                        ShowContinueError("is below condenser inlet temperature of [" + General::RoundSigDigits(CondInletTemp, 1) + "].");
                        ShowContinueErrorTimeStamp("");
                        ShowContinueError(" Reset reference temperature to one greater than the inlet temperature ");
                    }
                    ShowRecurringSevereErrorAtEnd("ChillerHeaterPerformance:Electric:EIR=\"" +
                                                      Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name +
                                                      "\": Reference temperature problems continue.",
                                                  Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRRefTempErrorIndex,
                                                  CondDeltaTemp,
                                                  CondDeltaTemp,
                                                  _,
                                                  "deltaC",
                                                  "deltaC");
                    QCondenser = 0.0;
                    IsLoadHeatRemaining = false;
                }

                if (ChillerHeaterNum > 1) {
                    // Operation mode needs to be set in a simultaneous clg/htg mode
                    // Always off even heating load remains if this CH is assumed to be off in the loop 1
                    if (Wrapper(WrapperNum).SimulClgDominant) {
                        if (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvapSimul == 0.0) {
                            CurrentMode = 0;
                            IsLoadHeatRemaining = false;
                        } else { // Heat recovery
                            CurrentMode = 3;
                        }
                    }
                } // End of simulataneous clg/htg mode detemination

            } else { // chiller heater is off
                IsLoadHeatRemaining = false;
                CondMassFlowRate = 0.0;
                EvapMassFlowRate = 0.0;
                CurrentMode = 0;
                if (Wrapper(WrapperNum).SimulClgDominant) {
                    if (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvapSimul > 0.0) {
                        CurrentMode = 4; // Simultaneous cooling dominant mode: 4
                    }
                } // End of mode determination
            }     // End of system operation determinatoin

            if (IsLoadHeatRemaining && CondMassFlowRate > 0.0 &&
                (ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).WrapperComp(CompNum).CHSchedPtr) > 0)) { // System is on
                // Operation mode
                if (Wrapper(WrapperNum).SimulHtgDominant) {
                    if (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvapSimul == 0.0) {
                        CurrentMode = 5; // No cooling necessary
                    } else {             // Heat recovery mode. Both chilled water and hot water loops are connected. No condenser flow.
                        CurrentMode = 3;
                    }
                }

                // Mode 3 and 5 use cooling side data stored from the chilled water loop
                // Mode 4 uses all data from the chilled water loop due to no heating demand
                if (Wrapper(WrapperNum).SimulClgDominant || CurrentMode == 3) {
                    CurrentMode = 3;
                    Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).HWLoopNum).FluidName,
                                               CondInletTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).HWLoopNum).FluidIndex,
                                               RoutineName);

                    QCondenser = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCondSimul;

                    if (Wrapper(WrapperNum).VariableFlowCH) { // Variable flow
                        Real64 CondMassFlowRateCalc = QCondenser / CondDeltaTemp / Cp;
                        if (CondMassFlowRateCalc > CondMassFlowRate) {
                            CondMassFlowRateCalc = CondMassFlowRate;
                            Real64 CondDeltaTempCalc = QCondenser / CondMassFlowRate / Cp;
                            if (CondDeltaTempCalc > CondDeltaTemp) { // Load to meet should be adjusted
                                QCondenser = CondMassFlowRate * Cp * CondDeltaTemp;
                            }
                        }
                        CondMassFlowRate = CondMassFlowRateCalc;
                    } else { // Constant flow control
                        Real64 CondDeltaTempCalc = QCondenser / CondMassFlowRate / Cp;
                        Real64 CondOutletTempCalc = CondDeltaTempCalc + CondInletTemp;
                        if (CondOutletTempCalc > CondOutletTemp) {
                            CondOutletTempCalc = CondOutletTemp;
                            QCondenser = CondMassFlowRate * Cp * CondDeltaTemp;
                        }
                        CondOutletTemp = CondOutletTempCalc;
                    }

                } else { // Either Mode 2 or 3 or 5
                    if (Wrapper(WrapperNum).SimulHtgDominant) {
                        CurrentMode = 5;
                    } else {
                        CurrentMode = 2;
                    }

                    ChillerCapFT = 0.0;
                    ChillerEIRFT = 0.0;
                    ChillerEIRFPLR = 0.0;

                    // Assign curve values to local data array
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCap = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCapClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCOP = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCOPClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefEvapOut =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefEvapOutClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondOut =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondOutClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).OptPartLoadRat =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).OptPartLoadRatClgHtg;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondMode =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondModeHeating;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTIDX =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTHeatingIDX;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFTIDX =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFTHeatingIDX;
                    Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRIDX =
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRHeatingIDX;

                    Real64 CondTempforCurve;               // Reference condenser temperature for the performance curve reading

                    if (Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondMode == "ENTERINGCONDENSER") {
                        CondTempforCurve = CondInletTemp;
                    } else if (Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondMode == "LEAVINGCONDENSER") {
                        CondTempforCurve = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefCondOutClgHtg; //! CondOutletTemp
                    } else {
                        ShowWarningError("ChillerHeaterPerformance:Electric:EIR \"" + Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name +
                                         "\":");
                        ShowContinueError("Chiller condensor temperature for curve fit are not decided, defalt value= cond_leaving (" +
                                          General::RoundSigDigits(ChillerCapFT, 3) + ").");
                        CondTempforCurve = DataLoopNode::Node(DataPlant::PlantLoop(Wrapper(WrapperNum).HWLoopNum).TempSetPointNodeNum).TempSetPoint;
                    }

                    Real64 MinPartLoadRat;                 // Min allowed operating fraction of full load
                    Real64 MaxPartLoadRat;                 // Max allowed operating fraction of full load

                    CurveManager::GetCurveMinMaxValues(Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRIDX, MinPartLoadRat, MaxPartLoadRat);
                    Real64 ChillerRefCap = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCap;
                    Real64 ReferenceCOP = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).RefCOP;
                    EvapOutletTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefEvapOutClgHtg;
                    Real64 TempLowLimitEout = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempLowLimitEvapOut;
                    Real64 EvapOutletTempSetPoint = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).TempRefEvapOutClgHtg;
                    ChillerCapFT =
                            CurveManager::CurveValue(Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTIDX, EvapOutletTempSetPoint, CondTempforCurve);

                    if (ChillerCapFT < 0) {
                        if (Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTError < 1 && !DataGlobals::WarmupFlag) {
                            ++Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTError;
                            ShowWarningError("ChillerHeaterPerformance:Electric:EIR \"" + Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name +
                                             "\":");
                            ShowContinueError(" ChillerHeater Capacity as a Function of Temperature curve output is negative (" +
                                              General::RoundSigDigits(ChillerCapFT, 3) + ").");
                            ShowContinueError(" Negative value occurs using an Evaporator Outlet Temp of " +
                                              General::RoundSigDigits(EvapOutletTempSetPoint, 1) + " and a Condenser Inlet Temp of " +
                                              General::RoundSigDigits(CondInletTemp, 1) + '.');
                            ShowContinueErrorTimeStamp(" Resetting curve output to zero and continuing simulation.");
                        } else if (!DataGlobals::WarmupFlag) {
                            ++Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTError;
                            ShowRecurringWarningErrorAtEnd(
                                "ChillerHeaterPerformance:Electric:EIR \"" + Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).Name +
                                    "\": ChillerHeater Capacity as a Function of Temperature curve output is negative warning continues...",
                                Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerCapFTErrorIndex,
                                ChillerCapFT,
                                ChillerCapFT);
                        }
                        ChillerCapFT = 0.0;
                    }

                    // Available chiller capacity as a function of temperature
                    Real64 AvailChillerCap = ChillerRefCap * ChillerCapFT;

                    Real64 PartLoadRat;                    // Operating part load ratio

                    // Part load ratio based on reference capacity and available chiller capacity
                    if (AvailChillerCap > 0) {
                        PartLoadRat = max(0.0, min((ChillerRefCap / AvailChillerCap), MaxPartLoadRat));
                    } else {
                        PartLoadRat = 0.0;
                    }

                    Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).HWLoopNum).FluidName,
                                               Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).HWLoopNum).FluidIndex,
                                               RoutineName);

                    // Calculate evaporator heat transfer
                    if (EvapMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                        QEvaporator = AvailChillerCap * PartLoadRat;
                        Real64 EvapDeltaTemp = QEvaporator / EvapMassFlowRate / Cp;
                        EvapOutletTemp = EvapInletTemp - EvapDeltaTemp;
                    }

                    // Check that the evaporator outlet temp honors both plant loop temp low limit and also the chiller low limit
                    if (EvapOutletTemp < TempLowLimitEout) {
                        if ((Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp - TempLowLimitEout) > DataPlant::DeltaTempTol) {
                            EvapOutletTemp = TempLowLimitEout;
                            Real64 EvapDeltaTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp - EvapOutletTemp;
                            QEvaporator = EvapMassFlowRate * Cp * EvapDeltaTemp;
                        } else {
                            EvapOutletTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp;
                            Real64 EvapDeltaTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp - EvapOutletTemp;
                            QEvaporator = EvapMassFlowRate * Cp * EvapDeltaTemp;
                        }
                    }

                    if (EvapOutletTemp < Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.TempMin) {
                        if ((Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp -
                             Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.TempMin) > DataPlant::DeltaTempTol) {
                            EvapOutletTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.TempMin;
                            Real64 EvapDeltaTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.TempMin - EvapOutletTemp;
                            QEvaporator = EvapMassFlowRate * Cp * EvapDeltaTemp;
                        } else {
                            EvapOutletTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.TempMin;
                            Real64 EvapDeltaTemp = Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.TempMin - EvapOutletTemp;
                            QEvaporator = EvapMassFlowRate * Cp * EvapDeltaTemp;
                        }
                    }

                    // Evaporator operates at full load
                    if (AvailChillerCap > 0.0) {
                        PartLoadRat = max(0.0, min((QEvaporator / AvailChillerCap), MaxPartLoadRat));
                    } else {
                        PartLoadRat = 0.0;
                    }

                    // Chiller cycles below minimum part load ratio, FRAC = amount of time chiller is ON during this time step
                    if (PartLoadRat < MinPartLoadRat) FRAC = min(1.0, (PartLoadRat / MinPartLoadRat));
                    if (FRAC <= 0.0) FRAC = 1.0; // CR 9303 COP reporting issue, it should be greater than zero in this routine
                    ChillerCyclingRatio = FRAC;

                    // Chiller is false loading below PLR = minimum unloading ratio, find PLR used for energy calculation
                    if (AvailChillerCap > 0.0) {
                        PartLoadRat = max(PartLoadRat, MinPartLoadRat);
                    } else {
                        PartLoadRat = 0.0;
                    }
                    // Evaporator part load ratio
                    ChillerPartLoadRatio = PartLoadRat;

                    // calculate the load due to false loading on chiller over and above water side load
                    ChillerFalseLoadRate = (AvailChillerCap * PartLoadRat * FRAC) - QEvaporator;
                    if (ChillerFalseLoadRate < DataHVACGlobals::SmallLoad) {
                        ChillerFalseLoadRate = 0.0;
                    }

                    ChillerEIRFT =
                        max(0.0, CurveManager::CurveValue(Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFTIDX, EvapOutletTemp, CondTempforCurve));
                    ChillerEIRFPLR = max(0.0, CurveManager::CurveValue(Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).ChillerEIRFPLRIDX, PartLoadRat));
                    CHPower = (AvailChillerCap / ReferenceCOP) * ChillerEIRFPLR * ChillerEIRFT * FRAC;
                    ActualCOP = (QEvaporator + ChillerFalseLoadRate) / CHPower;
                    QCondenser = CHPower * Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).OpenMotorEff + QEvaporator + ChillerFalseLoadRate;

                    // Determine heating load for this heater and pass the remaining load to the next chiller heater
                    Real64 CondenserCapMin = QCondenser * MinPartLoadRat;
                    Real64 HeatingLoadToMeet = min(QCondenser, max(std::abs(CondenserLoad), CondenserCapMin));

                    // Set load this chiller heater should meet and temperatures given
                    QCondenser = min(HeatingLoadToMeet, QCondenser);

                    Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(Wrapper(WrapperNum).HWLoopNum).FluidName,
                                               CondInletTemp,
                                               DataPlant::PlantLoop(Wrapper(WrapperNum).HWLoopNum).FluidIndex,
                                               RoutineNameElecEIRChiller);

                    // Calculate temperatures for constant flow and mass flow rate for variable flow
                    // Limit mass for this chiller heater to the available mass at given temperature conditions
                    // when mass calculated to meet the load is greater than the maximum available
                    // then recalculate heating load this chiller heater can meet
                    if (CurrentMode == 2 || Wrapper(WrapperNum).SimulHtgDominant) {
                        if (CondMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance && CondDeltaTemp > 0.0) {
                            if (Wrapper(WrapperNum).VariableFlowCH) { // Variable flow
                                Real64 CondMassFlowRateCalc = QCondenser / CondDeltaTemp / Cp;
                                if (CondMassFlowRateCalc > CondMassFlowRate) {
                                    CondMassFlowRateCalc = CondMassFlowRate;
                                    Real64 CondDeltaTempCalc = QCondenser / CondMassFlowRate / Cp;
                                    if (CondDeltaTempCalc > CondDeltaTemp) { // Load to meet should be adjusted
                                        QCondenser = CondMassFlowRate * Cp * CondDeltaTemp;
                                    }
                                }
                                CondMassFlowRate = CondMassFlowRateCalc;
                            } else { // Constant Flow at a fixed flow rate and capacity
                                Real64 CondDeltaTempCalc = QCondenser / CondMassFlowRate / Cp;
                                Real64 CondOutletTempCalc = CondDeltaTempCalc + CondInletTemp;
                                if (CondOutletTempCalc > CondOutletTemp) { // Load to meet should be adjusted
                                    CondOutletTempCalc = CondOutletTemp;
                                    QCondenser = CondMassFlowRate * Cp * CondDeltaTemp;
                                }
                                CondOutletTemp = CondOutletTempCalc;
                            }
                        } else {
                            QCondenser = 0.0;
                            CondOutletTemp = CondInletTemp;
                        }
                    }

                } // End of calculation depending on the modes

                // Determine load next chiller heater meets
                if (CondenserLoad < QCondenser) { // Heating load is met by this chiller heater
                    CondenserLoad = 0.0;
                } else {
                    CondenserLoad -= QCondenser;
                }

                if (QCondenser == 0.0) {
                    CurrentMode = 0;
                    ChillerPartLoadRatio = 0.0;
                    ChillerCyclingRatio = 0.0;
                    ChillerFalseLoadRate = 0.0;
                    EvapMassFlowRate = 0.0;
                    CondMassFlowRate = 0.0;
                    CHPower = 0.0;
                    QEvaporator = 0.0;
                    EvapOutletTemp = EvapInletTemp;
                    CondOutletTemp = CondInletTemp;
                    CondenserLoad = 0.0;
                }

                // Heat recovery or cooling dominant modes need to use the evaporator side information
                if (CurrentMode == 3 || CurrentMode == 4) {
                    ChillerPartLoadRatio = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerPartLoadRatioSimul;
                    ChillerCyclingRatio = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCyclingRatioSimul;
                    ChillerFalseLoadRate = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoadRateSimul;
                    ChillerCapFT = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCapFTSimul;
                    ChillerEIRFT = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFTSimul;
                    ChillerEIRFPLR = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFPLRSimul;
                    QEvaporator = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvapSimul;
                    EvapOutletTemp = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTempSimul;
                    EvapInletTemp = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapInletTempSimul;
                    EvapMassFlowRate = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapmdotSimul;
                    if (Wrapper(WrapperNum).SimulClgDominant) {
                        CHPower = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPowerSimul;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingPower = 0.0;
                    }
                }
            }

            // Check if it is mode 4, then skip binding local variables
            if (CurrentMode == 4) {
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CurrentMode = CurrentMode;
            } else {
                Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.MassFlowRate = EvapMassFlowRate;
                Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondOutletNode.MassFlowRate = CondMassFlowRate;
                Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.Temp = EvapOutletTemp;
                Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp = EvapInletTemp;
                Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondOutletNode.Temp = CondOutletTemp;
                Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.Temp = CondInletTemp;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CurrentMode = CurrentMode;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerPartLoadRatio = ChillerPartLoadRatio;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCyclingRatio = ChillerCyclingRatio;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoadRate = ChillerFalseLoadRate;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCapFT = ChillerCapFT;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFT = ChillerEIRFT;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFPLR = ChillerEIRFPLR;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPower = CoolingPower;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingPower = CHPower;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvap = QEvaporator;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCond = QCondenser;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTemp = EvapOutletTemp;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapInletTemp = EvapInletTemp;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTemp = CondOutletTemp;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondInletTemp = CondInletTemp;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Evapmdot = EvapMassFlowRate;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot = CondMassFlowRate;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ActualCOP = ActualCOP;
            }
        }
    }

    void
    CalcWrapperModel(int const WrapperNum, Real64 &MyLoad, bool const RunFlag, bool const FirstIteration, int const EquipFlowCtrl, int const LoopNum)
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Daeho Kang, PNNL
        //       DATE WRITTEN   Feb 2013
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        //  Calculate node information connected to plant & condenser loop

        // METHODOLOGY EMPLOYED:
        //  Use empirical curve fits to model performance at off-reference conditions

        Real64 CurHeatingLoad = 0.0;             // Total heating load chiller heater bank (wrapper) meets
        Real64 CHWOutletTemp;              // Chiller heater bank chilled water outlet temperature
        Real64 CHWOutletMassFlowRate;      // Chiller heater bank chilled water outlet mass flow rate
        Real64 HWOutletTemp;               // Chiller heater bank hot water outlet temperature
        Real64 GLHEOutletTemp;             // Chiller heater bank condenser loop outlet temperature
        Real64 GLHEOutletMassFlowRate;     // Chiller heater bank condenser loop outlet mass flow rate
        Real64 WrapperElecPowerCool(0.0);  // Chiller heater bank total cooling electricity [W]
        Real64 WrapperElecPowerHeat(0.0);  // Chiller heater bank total heating electricity [W]
        Real64 WrapperCoolRate(0.0);       // Chiller heater bank total cooling rate [W]
        Real64 WrapperHeatRate(0.0);       // Chiller heater bank total heating rate [W]
        Real64 WrapperGLHERate(0.0);       // Chiller heater bank total condenser heat transfer rate [W]
        Real64 WrapperElecEnergyCool(0.0); // Chiller heater bank total electric cooling energy [J]
        Real64 WrapperElecEnergyHeat(0.0); // Chiller heater bank total electric heating energy [J]
        Real64 WrapperCoolEnergy(0.0);     // Chiller heater bank total cooling energy [J]
        Real64 WrapperHeatEnergy(0.0);     // Chiller heater bank total heating energy [J]
        Real64 WrapperGLHEEnergy(0.0);     // Chiller heater bank total condenser heat transfer energy [J]

        int OpMode = 0;

        // Chiller heater bank chilled water inlet mass flow rate
        Real64 CHWInletMassFlowRate = 0.0;

        Real64 HWInletMassFlowRate = 0.0;
        Real64 GLHEInletMassFlowRate = 0.0;
        Real64 CHWInletTemp = DataLoopNode::Node(Wrapper(WrapperNum).CHWInletNodeNum).Temp;

        // Chiller heater bank hot water inlet temperature
        Real64 HWInletTemp = DataLoopNode::Node(Wrapper(WrapperNum).HWInletNodeNum).Temp;

        // Chiller heater bank condenser loop inlet temperature
        Real64 GLHEInletTemp = DataLoopNode::Node(Wrapper(WrapperNum).GLHEInletNodeNum).Temp;

        int ChillerHeaterNums = Wrapper(WrapperNum).ChillerHeaterNums;

        Real64 CurCoolingLoad = 0.0;             // Total cooling load chiller heater bank (wrapper) meets

        // Initiate loads and inlet temperatures each loop
        if (LoopNum == Wrapper(WrapperNum).CWLoopNum) {
            CHWInletMassFlowRate = DataLoopNode::Node(Wrapper(WrapperNum).CHWInletNodeNum).MassFlowRateMaxAvail;
            HWInletMassFlowRate = DataLoopNode::Node(Wrapper(WrapperNum).HWInletNodeNum).MassFlowRate;
            GLHEInletMassFlowRate = DataLoopNode::Node(Wrapper(WrapperNum).GLHEInletNodeNum).MassFlowRateMaxAvail;
            int LoopSideNum = Wrapper(WrapperNum).CWLoopSideNum;
            Wrapper(WrapperNum).WrapperCoolingLoad = 0.0;
            CurCoolingLoad = std::abs(MyLoad);
            Wrapper(WrapperNum).WrapperCoolingLoad = CurCoolingLoad;
            // Set actual mass flow rate at the nodes when it's locked
            if (DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 1) {
                CHWInletMassFlowRate = DataLoopNode::Node(Wrapper(WrapperNum).CHWInletNodeNum).MassFlowRate;
            }
            if (CHWInletMassFlowRate == 0.0) GLHEInletMassFlowRate = 0.0;

        } else if (LoopNum == Wrapper(WrapperNum).HWLoopNum) {
            CHWInletMassFlowRate = DataLoopNode::Node(Wrapper(WrapperNum).CHWInletNodeNum).MassFlowRate;
            HWInletMassFlowRate = DataLoopNode::Node(Wrapper(WrapperNum).HWInletNodeNum).MassFlowRateMaxAvail;
            GLHEInletMassFlowRate = DataLoopNode::Node(Wrapper(WrapperNum).GLHEInletNodeNum).MassFlowRateMaxAvail;
            int LoopSideNum = Wrapper(WrapperNum).HWLoopSideNum;
            Wrapper(WrapperNum).WrapperHeatingLoad = 0.0;
            CurHeatingLoad = MyLoad;
            Wrapper(WrapperNum).WrapperHeatingLoad = CurHeatingLoad;
            // Set actual mass flow rate at the nodes when it's locked
            if (DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 1) {
                HWInletMassFlowRate = DataLoopNode::Node(Wrapper(WrapperNum).HWInletNodeNum).MassFlowRate;
            }
            if (HWInletMassFlowRate == 0.0) GLHEInletMassFlowRate = 0.0;
        }

        if (LoopNum == Wrapper(WrapperNum).CWLoopNum) {
            if (Wrapper(WrapperNum).ControlMode == SmartMixing) {
                if (CurCoolingLoad > 0.0 && CHWInletMassFlowRate > 0.0 && GLHEInletMassFlowRate > 0) {

                    CalcChillerModel(WrapperNum,
                                     OpMode,
                                     MyLoad,
                                     RunFlag,
                                     FirstIteration,
                                     EquipFlowCtrl,
                                     LoopNum);
                    UpdateChillerRecords(WrapperNum);

                    // Initialize local variables only for calculating mass-weighed temperatures
                    CHWOutletTemp = 0.0;
                    GLHEOutletTemp = 0.0;
                    CHWOutletMassFlowRate = 0.0;
                    GLHEOutletMassFlowRate = 0.0;

                    for (int ChillerHeaterNum = 1; ChillerHeaterNum <= ChillerHeaterNums; ++ChillerHeaterNum) {

                        // Calculated mass flow rate used by individual chiller heater and bypasses
                        CHWOutletMassFlowRate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Evapmdot;
                        CHWOutletTemp += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTemp *
                                         (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Evapmdot / CHWInletMassFlowRate);
                        WrapperElecPowerCool += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPower;
                        WrapperCoolRate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvap;
                        WrapperElecEnergyCool += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingEnergy;
                        WrapperCoolEnergy += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergy;
                        if (GLHEInletMassFlowRate > 0.0) {
                            GLHEOutletMassFlowRate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot;
                            if (GLHEOutletMassFlowRate > GLHEInletMassFlowRate) GLHEOutletMassFlowRate = GLHEInletMassFlowRate;
                            GLHEOutletTemp += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTemp *
                                              (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot / GLHEInletMassFlowRate);
                            WrapperGLHERate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCond;
                            WrapperGLHEEnergy += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergy;
                        } else {
                            GLHEInletMassFlowRate = 0.0;
                            GLHEOutletMassFlowRate = 0.0;
                            GLHEOutletTemp = GLHEInletTemp;
                            WrapperGLHERate = 0.0;
                            WrapperGLHEEnergy = 0.0;
                        }
                    } // End of summation of mass flow rates and mass weighted temperatrue

                    // Calculate temperatures for the mixed flows in the chiller bank
                    Real64 CHWBypassMassFlowRate = CHWInletMassFlowRate - CHWOutletMassFlowRate;
                    if (CHWBypassMassFlowRate > 0.0) {
                        CHWOutletTemp += CHWInletTemp * CHWBypassMassFlowRate / CHWInletMassFlowRate;
                    } else {
                        // CHWOutletTemp = CHWOutletTemp; // Self-assignment commented out
                    }

                    if (GLHEInletMassFlowRate > 0.0) {
                        Real64 GLHEBypassMassFlowRate = GLHEInletMassFlowRate - GLHEOutletMassFlowRate;
                        if (GLHEBypassMassFlowRate > 0.0) {
                            GLHEOutletTemp += GLHEInletTemp * GLHEBypassMassFlowRate / GLHEInletMassFlowRate;
                        } else {
                            // GLHEOutletTemp = GLHEOutletTemp; // Self-assignment commented out
                        }
                    } else {
                        GLHEOutletTemp = GLHEInletTemp;
                    }

                    HWOutletTemp = HWInletTemp;

                    if (ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).SchedPtr) > 0) {
                        WrapperElecPowerCool += (Wrapper(WrapperNum).AncillaryPower * ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).SchedPtr));
                    }

                    DataLoopNode::Node(Wrapper(WrapperNum).CHWOutletNodeNum).Temp = CHWOutletTemp;
                    DataLoopNode::Node(Wrapper(WrapperNum).HWOutletNodeNum).Temp = HWOutletTemp;
                    DataLoopNode::Node(Wrapper(WrapperNum).GLHEOutletNodeNum).Temp = GLHEOutletTemp;

                } else {

                    // Initialize local variables
                    CHWOutletTemp = CHWInletTemp;
                    HWOutletTemp = HWInletTemp;
                    GLHEOutletTemp = GLHEInletTemp;

                    for (int ChillerHeaterNum = 1; ChillerHeaterNum <= ChillerHeaterNums; ++ChillerHeaterNum) {
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.MassFlowRate = 0.0;
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondOutletNode.MassFlowRate = 0.0;
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.Temp = CHWInletTemp;
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp = CHWInletTemp;
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondOutletNode.Temp = GLHEInletTemp;
                        Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.Temp = GLHEInletTemp;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CurrentMode = 0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerPartLoadRatio = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCyclingRatio = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoadRate = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCapFT = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFT = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFPLR = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPower = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingPower = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvap = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCond = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTemp = CHWOutletTemp;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapInletTemp = CHWInletTemp;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTemp = GLHEOutletTemp;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondInletTemp = GLHEInletTemp;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Evapmdot = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoad = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingEnergy = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingEnergy = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergy = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergy = 0.0;
                        Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ActualCOP = 0.0;
                    }
                }

                if (Wrapper(WrapperNum).SimulHtgDominant || Wrapper(WrapperNum).SimulClgDominant) {
                    DataLoopNode::Node(Wrapper(WrapperNum).CHWOutletNodeNum).Temp = CHWOutletTemp;
                    WrapperReport(WrapperNum).CHWInletTempSimul = CHWInletTemp;
                    WrapperReport(WrapperNum).CHWOutletTempSimul = CHWOutletTemp;
                    WrapperReport(WrapperNum).CHWmdotSimul = CHWInletMassFlowRate;
                    WrapperReport(WrapperNum).GLHEInletTempSimul = GLHEInletTemp;
                    WrapperReport(WrapperNum).GLHEOutletTempSimul = GLHEOutletTemp;
                    WrapperReport(WrapperNum).GLHEmdotSimul = GLHEInletMassFlowRate;
                    WrapperReport(WrapperNum).TotElecCoolingSimul = WrapperElecEnergyCool;
                    WrapperReport(WrapperNum).CoolingEnergySimul = WrapperCoolEnergy;
                    WrapperReport(WrapperNum).TotElecCoolingPwrSimul = WrapperElecPowerCool;
                    WrapperReport(WrapperNum).CoolingRateSimul = WrapperCoolRate;

                } else {

                    DataLoopNode::Node(Wrapper(WrapperNum).CHWOutletNodeNum).Temp = CHWOutletTemp;
                    DataLoopNode::Node(Wrapper(WrapperNum).HWOutletNodeNum).Temp = HWOutletTemp;
                    DataLoopNode::Node(Wrapper(WrapperNum).GLHEOutletNodeNum).Temp = GLHEOutletTemp;
                    WrapperReport(WrapperNum).CHWInletTemp = CHWInletTemp;
                    WrapperReport(WrapperNum).CHWOutletTemp = CHWOutletTemp;
                    WrapperReport(WrapperNum).HWInletTemp = HWInletTemp;
                    WrapperReport(WrapperNum).HWOutletTemp = HWOutletTemp;
                    WrapperReport(WrapperNum).GLHEInletTemp = GLHEInletTemp;
                    WrapperReport(WrapperNum).GLHEOutletTemp = GLHEOutletTemp;
                    WrapperReport(WrapperNum).CHWmdot = CHWInletMassFlowRate;
                    WrapperReport(WrapperNum).HWmdot = HWInletMassFlowRate;
                    WrapperReport(WrapperNum).GLHEmdot = GLHEInletMassFlowRate;
                    WrapperReport(WrapperNum).TotElecCooling = WrapperElecEnergyCool;
                    WrapperReport(WrapperNum).TotElecHeating = WrapperElecEnergyHeat;
                    WrapperReport(WrapperNum).CoolingEnergy = WrapperCoolEnergy;
                    WrapperReport(WrapperNum).HeatingEnergy = WrapperHeatEnergy;
                    WrapperReport(WrapperNum).GLHEEnergy = WrapperGLHEEnergy;
                    WrapperReport(WrapperNum).TotElecCoolingPwr = WrapperElecPowerCool;
                    WrapperReport(WrapperNum).TotElecHeatingPwr = WrapperElecPowerHeat;
                    WrapperReport(WrapperNum).CoolingRate = WrapperCoolRate;
                    WrapperReport(WrapperNum).HeatingRate = WrapperHeatRate;
                    WrapperReport(WrapperNum).GLHERate = WrapperGLHERate;
                }
                PlantUtilities::SetComponentFlowRate(CHWInletMassFlowRate,
                                                     Wrapper(WrapperNum).CHWInletNodeNum,
                                                     Wrapper(WrapperNum).CHWOutletNodeNum,
                                     Wrapper(WrapperNum).CWLoopNum,
                                     Wrapper(WrapperNum).CWLoopSideNum,
                                     Wrapper(WrapperNum).CWBranchNum,
                                     Wrapper(WrapperNum).CWCompNum);

                PlantUtilities::SetComponentFlowRate(HWInletMassFlowRate,
                                                     Wrapper(WrapperNum).HWInletNodeNum,
                                                     Wrapper(WrapperNum).HWOutletNodeNum,
                                     Wrapper(WrapperNum).HWLoopNum,
                                     Wrapper(WrapperNum).HWLoopSideNum,
                                     Wrapper(WrapperNum).HWBranchNum,
                                     Wrapper(WrapperNum).HWCompNum);

                PlantUtilities::SetComponentFlowRate(GLHEInletMassFlowRate,
                                                     Wrapper(WrapperNum).GLHEInletNodeNum,
                                                     Wrapper(WrapperNum).GLHEOutletNodeNum,
                                     Wrapper(WrapperNum).GLHELoopNum,
                                     Wrapper(WrapperNum).GLHELoopSideNum,
                                     Wrapper(WrapperNum).GLHEBranchNum,
                                     Wrapper(WrapperNum).GLHECompNum);

            } // End of cooling

        } else if (LoopNum == Wrapper(WrapperNum).HWLoopNum) {    // Hot water loop
            if (Wrapper(WrapperNum).ControlMode == SmartMixing) { // Chiller heater component
                if (CurHeatingLoad > 0.0 && HWInletMassFlowRate > 0.0) {

                    CalcChillerHeaterModel(WrapperNum,
                                           OpMode,
                                           MyLoad,
                                           RunFlag,
                                           FirstIteration,
                                           EquipFlowCtrl,
                                           LoopNum); // Autodesk:Uninit OpMode was uninitialized
                    UpdateChillerHeaterRecords(WrapperNum);

                    // Calculate individual CH units's temperatures and mass flow rates
                    CHWOutletTemp = 0.0;
                    HWOutletTemp = 0.0;
                    GLHEOutletTemp = 0.0;
                    CHWOutletMassFlowRate = 0.0;
                    Real64 HWOutletMassFlowRate = 0.0;
                    GLHEOutletMassFlowRate = 0.0;

                    if (Wrapper(WrapperNum).SimulHtgDominant || Wrapper(WrapperNum).SimulClgDominant) {
                        if (Wrapper(WrapperNum).SimulClgDominant) {
                            for (int ChillerHeaterNum = 1; ChillerHeaterNum <= ChillerHeaterNums; ++ChillerHeaterNum) {
                                int CurrentMode = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CurrentMode;
                                CHWInletTemp = WrapperReport(WrapperNum).CHWInletTempSimul;
                                GLHEInletTemp = WrapperReport(WrapperNum).GLHEInletTempSimul;
                                CHWInletMassFlowRate = WrapperReport(WrapperNum).CHWmdotSimul;
                                GLHEInletMassFlowRate = WrapperReport(WrapperNum).GLHEmdotSimul;

                                if (CurrentMode != 0) {     // This chiller heater unit is on
                                    if (CurrentMode == 3) { // Heat recovery mode. Both chilled water and hot water connections
                                        CHWOutletMassFlowRate += Wrapper(WrapperNum)
                                                                     .ChillerHeaterReport(ChillerHeaterNum)
                                                                     .EvapmdotSimul; // Wrapper evaporator side to plant chilled water loop
                                        HWOutletMassFlowRate += Wrapper(WrapperNum)
                                                                    .ChillerHeaterReport(ChillerHeaterNum)
                                                                    .Condmdot; // Wrapper condenser side to plant hot water loop
                                        if (HWInletMassFlowRate > 0.0) {
                                            HWOutletTemp += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTemp *
                                                            (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot /
                                                             HWInletMassFlowRate); // Only calculate in the heat recovery mode
                                        } else {
                                            HWOutletTemp = HWInletTemp;
                                        }
                                    } else { // Mode 4. Cooling-only mode with other heat recovery units. Condenser flows.
                                        CHWOutletMassFlowRate += Wrapper(WrapperNum)
                                                                     .ChillerHeaterReport(ChillerHeaterNum)
                                                                     .EvapmdotSimul; // Wrapper evaporator side to plant chilled water loop
                                        // Sum condenser node mass flow rates and mass weighed temperatures
                                        if (GLHEInletMassFlowRate > 0.0) {
                                            GLHEOutletMassFlowRate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondmdotSimul;
                                            if (GLHEOutletMassFlowRate > GLHEInletMassFlowRate) GLHEOutletMassFlowRate = GLHEInletMassFlowRate;
                                            GLHEOutletTemp +=
                                                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTempSimul *
                                                (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondmdotSimul / GLHEInletMassFlowRate);
                                            WrapperGLHERate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCondSimul;
                                            WrapperGLHEEnergy += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergySimul;
                                        } else {
                                            GLHEInletMassFlowRate = 0.0;
                                            GLHEOutletMassFlowRate = 0.0;
                                            GLHEOutletTemp = GLHEInletTemp;
                                            WrapperGLHERate = 0.0;
                                            WrapperGLHEEnergy = 0.0;
                                        }
                                    }
                                } else { // This chiller heater is off
                                    // Check if any unit is cooling only mode
                                    if (ChillerHeaterNum == ChillerHeaterNums) { // All units are heat revocery mode. No condenser flow
                                        GLHEOutletMassFlowRate = 0.0;
                                        GLHEInletMassFlowRate = 0.0;
                                        GLHEOutletTemp = GLHEInletTemp;
                                    } else { // At leaset, one of chiller heater units is cooling-only mode
                                             // GLHEOutletMassFlowRate = GLHEOutletMassFlowRate; // Self-assignment commented out
                                             // GLHEOutletTemp = GLHEOutletTemp; // Self-assignment commented out
                                    }
                                }
                                // Calculate mass weighed chilled water temperatures
                                if (CHWInletMassFlowRate > 0.0) {
                                    CHWOutletTemp += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTempSimul *
                                                     (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapmdotSimul / CHWInletMassFlowRate);
                                } else {
                                    CHWOutletTemp = CHWInletTemp;
                                }

                                WrapperElecPowerCool +=
                                    Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPowerSimul; // Cooling electricity
                                WrapperCoolRate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvapSimul;
                                WrapperElecEnergyCool += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingEnergySimul;
                                WrapperCoolEnergy += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergySimul;
                                // Avoid double counting wrapper energy use
                                WrapperElecPowerHeat = 0.0;
                                WrapperHeatRate = 0.0;
                                WrapperHeatEnergy = 0.0;
                            }

                            // Calculate chilled water temperature
                            if (CHWInletMassFlowRate > 0.0) {
                                Real64 CHWBypassMassFlowRate = CHWInletMassFlowRate - CHWOutletMassFlowRate;
                                if (CHWBypassMassFlowRate > 0.0) {
                                    CHWOutletTemp += CHWInletTemp * CHWBypassMassFlowRate / CHWInletMassFlowRate;
                                } else { // No bypass withnin a wrapper
                                         // CHWOutletTemp = CHWOutletTemp; // Self-assignment commented out
                                }
                            } else {
                                CHWOutletTemp = CHWInletTemp;
                            }
                            // Calculate hot water outlet temperature
                            if (HWInletMassFlowRate > 0.0) {
                                Real64 HWBypassMassFlowRate = HWInletMassFlowRate - HWOutletMassFlowRate;
                                if (HWBypassMassFlowRate > 0.0) {
                                    HWOutletTemp += HWInletTemp * HWBypassMassFlowRate / HWInletMassFlowRate;
                                } else {
                                    // HWOutletTemp = HWOutletTemp; // Self-assignment commented out
                                }
                            } else {
                                HWOutletTemp = HWInletTemp;
                            }
                            // Calculate condenser outlet temperature
                            if (GLHEInletMassFlowRate > 0.0) {
                                Real64 GLHEBypassMassFlowRate = GLHEInletMassFlowRate - GLHEOutletMassFlowRate;
                                if (GLHEBypassMassFlowRate > 0.0) {
                                    GLHEOutletTemp += GLHEInletTemp * GLHEBypassMassFlowRate / GLHEInletMassFlowRate;
                                } else {
                                    // GLHEOutletTemp = GLHEOutletTemp; // Self-assignment commented out
                                }
                            } else {
                                GLHEOutletTemp = GLHEInletTemp;
                            }

                            // Add ancilliary power if scheduled
                            if (ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).SchedPtr) > 0) {
                                WrapperElecPowerCool += (Wrapper(WrapperNum).AncillaryPower * ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).SchedPtr));
                            }

                            // Electricity should be counted once for cooling in this mode
                            WrapperElecEnergyHeat = 0.0;

                        } else if (Wrapper(WrapperNum).SimulHtgDominant) { // Heating dominant simultaneous clg/htg mode

                            for (int ChillerHeaterNum = 1; ChillerHeaterNum <= ChillerHeaterNums; ++ChillerHeaterNum) {
                                // Set temperatures and mass flow rates for the cooling side
                                int CurrentMode = Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CurrentMode;
                                CHWInletTemp = WrapperReport(WrapperNum).CHWInletTempSimul;
                                CHWInletMassFlowRate = WrapperReport(WrapperNum).CHWmdotSimul;

                                if (CurrentMode != 0) {     // This chiller heater unit is on
                                    if (CurrentMode == 3) { // Heat recovery mode. Both chilled water and hot water connections
                                        CHWOutletMassFlowRate += Wrapper(WrapperNum)
                                                                     .ChillerHeaterReport(ChillerHeaterNum)
                                                                     .EvapmdotSimul; // Wrapper evaporator side to plant chilled water loop
                                        HWOutletMassFlowRate += Wrapper(WrapperNum)
                                                                    .ChillerHeaterReport(ChillerHeaterNum)
                                                                    .Condmdot; // Wrapper condenser side to plant hot water loop
                                        if (CHWInletMassFlowRate > 0.0) {
                                            CHWOutletTemp += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTempSimul *
                                                             (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapmdotSimul /
                                                              CHWInletMassFlowRate); // Only need to calculate in the heat recovery mode
                                        } else {
                                            CHWOutletTemp = CHWInletTemp;
                                        }
                                    } else { // Mode 5. Heating only mode with other heat recovery units
                                        HWOutletMassFlowRate += Wrapper(WrapperNum)
                                                                    .ChillerHeaterReport(ChillerHeaterNum)
                                                                    .Condmdot; // Wrapper condenser side to plant hot water loop
                                        if (GLHEInletMassFlowRate > 0.0) {
                                            GLHEOutletMassFlowRate += Wrapper(WrapperNum)
                                                                          .ChillerHeaterReport(ChillerHeaterNum)
                                                                          .Evapmdot; // Wrapper evaporator side to plant condenser loop
                                            if (GLHEOutletMassFlowRate > GLHEInletMassFlowRate) GLHEOutletMassFlowRate = GLHEInletMassFlowRate;
                                            GLHEOutletTemp +=
                                                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTemp *
                                                (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Evapmdot / GLHEInletMassFlowRate);
                                            WrapperGLHERate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvap;
                                            WrapperGLHEEnergy += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergy;
                                        } else {
                                            GLHEInletMassFlowRate = 0.0;
                                            GLHEOutletMassFlowRate = 0.0;
                                            GLHEOutletTemp = GLHEInletTemp;
                                            WrapperGLHERate = 0.0;
                                            WrapperGLHEEnergy = 0.0;
                                        }
                                    } // End of heat recovery mode

                                } else { // This chiller heater is off

                                    // Check if any unit is heating only mode
                                    if (ChillerHeaterNum == ChillerHeaterNums) { // All are heat revocery mode. No condenser flow
                                        GLHEOutletMassFlowRate = 0.0;
                                        GLHEInletMassFlowRate = 0.0;
                                        GLHEOutletTemp = GLHEInletTemp;
                                    } else { // At leaset, one of chiller heater units is heating only mode
                                             // GLHEOutletMassFlowRate = GLHEOutletMassFlowRate; // Self-assignment commented out
                                             // GLHEOutletTemp = GLHEOutletTemp; // Self-assignment commented out
                                    }
                                }

                                // Calculate mass weighed hot water temperatures
                                if (HWInletMassFlowRate > 0.0) {
                                    HWOutletTemp += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTemp *
                                                    (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot /
                                                     HWInletMassFlowRate); // Always heating as long as heating load remains
                                } else {
                                    HWOutletTemp = HWInletTemp;
                                }

                                WrapperElecPowerHeat += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingPower;
                                WrapperHeatRate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCond;
                                WrapperElecEnergyHeat += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingEnergy;
                                WrapperHeatEnergy += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergy;

                                // Avoid double counting wrapper energy use
                                WrapperElecPowerCool = 0.0;
                                WrapperCoolRate = 0.0;
                            }
                            // Calculate chilled water outlet temperature
                            if (CHWInletMassFlowRate > 0.0) {
                                Real64 CHWBypassMassFlowRate = CHWInletMassFlowRate - CHWOutletMassFlowRate;
                                if (CHWBypassMassFlowRate > 0.0) {
                                    CHWOutletTemp += CHWInletTemp * CHWBypassMassFlowRate / CHWInletMassFlowRate;
                                } else { // No bypass withnin a wrapper
                                         // CHWOutletTemp = CHWOutletTemp; // Self-assignment commented out
                                }
                            } else {
                                CHWOutletTemp = CHWInletTemp;
                            }
                            // Calculate hot water outlet temperature
                            if (HWInletMassFlowRate > 0.0) {
                                Real64 HWBypassMassFlowRate = HWInletMassFlowRate - HWOutletMassFlowRate;
                                if (HWBypassMassFlowRate > 0.0) {
                                    HWOutletTemp += HWInletTemp * HWBypassMassFlowRate / HWInletMassFlowRate;
                                } else {
                                    // HWOutletTemp = HWOutletTemp; // Self-assignment commented out
                                }
                            } else {
                                HWOutletTemp = HWInletTemp;
                            }
                            // Calculate condenser outlet temperature
                            if (GLHEInletMassFlowRate > 0.0) {
                                Real64 GLHEBypassMassFlowRate = GLHEInletMassFlowRate - GLHEOutletMassFlowRate;
                                if (GLHEBypassMassFlowRate > 0.0) {
                                    GLHEOutletTemp += GLHEInletTemp * GLHEBypassMassFlowRate / GLHEInletMassFlowRate;
                                } else {
                                    // GLHEOutletTemp = GLHEOutletTemp; // Self-assignment commented out
                                }
                            } else {
                                GLHEOutletTemp = GLHEInletTemp;
                            }

                            // Check if ancilliary power is used
                            if (ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).SchedPtr) > 0) {
                                WrapperElecPowerHeat += (Wrapper(WrapperNum).AncillaryPower * ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).SchedPtr));
                            }

                            // Electricity should be counted once
                            WrapperElecEnergyCool = 0.0;

                        } // End of simultaneous clg/htg mode calculations

                    } else { // Heating only mode (mode 2)

                        for (int ChillerHeaterNum = 1; ChillerHeaterNum <= ChillerHeaterNums; ++ChillerHeaterNum) {
                            HWOutletMassFlowRate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot;
                            HWOutletTemp += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTemp *
                                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot / HWInletMassFlowRate;
                            WrapperElecPowerHeat += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingPower;
                            WrapperHeatRate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCond;
                            WrapperElecEnergyHeat += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingEnergy;
                            WrapperHeatEnergy += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergy;

                            if (GLHEInletMassFlowRate > 0.0) {
                                GLHEOutletMassFlowRate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Evapmdot;
                                if (GLHEOutletMassFlowRate > GLHEInletMassFlowRate) GLHEOutletMassFlowRate = GLHEInletMassFlowRate;
                                GLHEOutletTemp += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTemp *
                                                  (Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Evapmdot / GLHEInletMassFlowRate);
                                WrapperGLHERate += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvap;
                                WrapperGLHEEnergy += Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergy;
                            } else { // No source water flow
                                GLHEOutletMassFlowRate = 0.0;
                                GLHEInletMassFlowRate = 0.0;
                                GLHEOutletTemp = GLHEInletTemp;
                                WrapperGLHERate = 0.0;
                                WrapperGLHEEnergy = 0.0;
                            }
                        }

                        // Calculate hot water outlet temperature
                        if (HWInletMassFlowRate > 0.0) {
                            Real64 HWBypassMassFlowRate = HWInletMassFlowRate - HWOutletMassFlowRate;
                            if (HWBypassMassFlowRate > 0.0) {
                                HWOutletTemp += HWInletTemp * HWBypassMassFlowRate / HWInletMassFlowRate;
                            } else {
                                // HWOutletTemp = HWOutletTemp; // Self-assignment commented out
                                if (HWOutletTemp > HWInletTemp) HWOutletTemp = HWInletTemp;
                            }
                        } else {
                            HWOutletTemp = HWInletTemp;
                        }

                        // Calculate condenser outlet temperature
                        if (GLHEInletMassFlowRate > 0.0) {
                            Real64 GLHEBypassMassFlowRate = GLHEInletMassFlowRate - GLHEOutletMassFlowRate;
                            if (GLHEBypassMassFlowRate > 0.0) {
                                GLHEOutletTemp += GLHEInletTemp * GLHEBypassMassFlowRate / GLHEInletMassFlowRate;
                            } else {
                                // GLHEOutletTemp = GLHEOutletTemp; // Self-assignment commented out
                            }
                        } else {
                            GLHEOutletTemp = GLHEInletTemp;
                        }

                        CHWOutletTemp = CHWInletTemp;

                        // Add ancilliary power if necessary
                        if (ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).SchedPtr) > 0) {
                            WrapperElecPowerHeat += (Wrapper(WrapperNum).AncillaryPower * ScheduleManager::GetCurrentScheduleValue(Wrapper(WrapperNum).SchedPtr));
                        }

                    } // End of calculations

                    PlantUtilities::SetComponentFlowRate(CHWInletMassFlowRate,
                                                         Wrapper(WrapperNum).CHWInletNodeNum,
                                                         Wrapper(WrapperNum).CHWOutletNodeNum,
                                         Wrapper(WrapperNum).CWLoopNum,
                                         Wrapper(WrapperNum).CWLoopSideNum,
                                         Wrapper(WrapperNum).CWBranchNum,
                                         Wrapper(WrapperNum).CWCompNum);

                    PlantUtilities::SetComponentFlowRate(HWInletMassFlowRate,
                                                         Wrapper(WrapperNum).HWInletNodeNum,
                                                         Wrapper(WrapperNum).HWOutletNodeNum,
                                         Wrapper(WrapperNum).HWLoopNum,
                                         Wrapper(WrapperNum).HWLoopSideNum,
                                         Wrapper(WrapperNum).HWBranchNum,
                                         Wrapper(WrapperNum).HWCompNum);

                    PlantUtilities::SetComponentFlowRate(GLHEInletMassFlowRate,
                                                         Wrapper(WrapperNum).GLHEInletNodeNum,
                                                         Wrapper(WrapperNum).GLHEOutletNodeNum,
                                         Wrapper(WrapperNum).GLHELoopNum,
                                         Wrapper(WrapperNum).GLHELoopSideNum,
                                         Wrapper(WrapperNum).GLHEBranchNum,
                                         Wrapper(WrapperNum).GLHECompNum);

                    // Local variables
                    WrapperReport(WrapperNum).CHWInletTemp = CHWInletTemp;
                    WrapperReport(WrapperNum).CHWOutletTemp = CHWOutletTemp;
                    WrapperReport(WrapperNum).HWInletTemp = HWInletTemp;
                    WrapperReport(WrapperNum).HWOutletTemp = HWOutletTemp;
                    WrapperReport(WrapperNum).GLHEInletTemp = GLHEInletTemp;
                    WrapperReport(WrapperNum).GLHEOutletTemp = GLHEOutletTemp;
                    WrapperReport(WrapperNum).CHWmdot = CHWInletMassFlowRate;
                    WrapperReport(WrapperNum).HWmdot = HWInletMassFlowRate;
                    WrapperReport(WrapperNum).GLHEmdot = GLHEInletMassFlowRate;
                    WrapperReport(WrapperNum).TotElecCooling = WrapperElecEnergyCool;
                    WrapperReport(WrapperNum).TotElecHeating = WrapperElecEnergyHeat;
                    WrapperReport(WrapperNum).CoolingEnergy = WrapperCoolEnergy;
                    WrapperReport(WrapperNum).HeatingEnergy = WrapperHeatEnergy;
                    WrapperReport(WrapperNum).GLHEEnergy = WrapperGLHEEnergy;
                    WrapperReport(WrapperNum).TotElecCoolingPwr = WrapperElecPowerCool;
                    WrapperReport(WrapperNum).TotElecHeatingPwr = WrapperElecPowerHeat;
                    WrapperReport(WrapperNum).CoolingRate = WrapperCoolRate;
                    WrapperReport(WrapperNum).HeatingRate = WrapperHeatRate;
                    WrapperReport(WrapperNum).GLHERate = WrapperGLHERate;

                    DataLoopNode::Node(Wrapper(WrapperNum).CHWOutletNodeNum).Temp = CHWOutletTemp;
                    DataLoopNode::Node(Wrapper(WrapperNum).HWOutletNodeNum).Temp = HWOutletTemp;
                    DataLoopNode::Node(Wrapper(WrapperNum).GLHEOutletNodeNum).Temp = GLHEOutletTemp;

                } else { // Central chiller heater system is off

                    CHWOutletTemp = CHWInletTemp;
                    HWOutletTemp = HWInletTemp;
                    GLHEOutletTemp = GLHEInletTemp;
                    DataLoopNode::Node(Wrapper(WrapperNum).CHWOutletNodeNum).Temp = CHWOutletTemp;
                    DataLoopNode::Node(Wrapper(WrapperNum).HWOutletNodeNum).Temp = HWOutletTemp;
                    DataLoopNode::Node(Wrapper(WrapperNum).GLHEOutletNodeNum).Temp = GLHEOutletTemp;

                    if (Wrapper(WrapperNum).WrapperCoolingLoad == 0.0 && !Wrapper(WrapperNum).SimulHtgDominant) {

                        for (int ChillerHeaterNum = 1; ChillerHeaterNum <= ChillerHeaterNums; ++ChillerHeaterNum) {
                            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.MassFlowRate = 0.0;
                            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondOutletNode.MassFlowRate = 0.0;
                            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapOutletNode.Temp = CHWInletTemp;
                            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).EvapInletNode.Temp = CHWInletTemp;
                            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondOutletNode.Temp = GLHEInletTemp;
                            Wrapper(WrapperNum).ChillerHeater(ChillerHeaterNum).CondInletNode.Temp = GLHEInletTemp;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CurrentMode = 0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerPartLoadRatio = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCyclingRatio = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoadRate = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerCapFT = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFT = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerEIRFPLR = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPower = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingPower = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvap = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCond = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapOutletTemp = CHWOutletTemp;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapInletTemp = CHWInletTemp;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondOutletTemp = GLHEOutletTemp;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondInletTemp = GLHEInletTemp;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Evapmdot = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).Condmdot = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoad = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingEnergy = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingEnergy = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergy = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergy = 0.0;
                            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ActualCOP = 0.0;
                        }

                        WrapperReport(WrapperNum).CHWInletTemp = CHWInletTemp;
                        WrapperReport(WrapperNum).CHWOutletTemp = CHWOutletTemp;
                        WrapperReport(WrapperNum).HWInletTemp = HWInletTemp;
                        WrapperReport(WrapperNum).HWOutletTemp = HWOutletTemp;
                        WrapperReport(WrapperNum).GLHEInletTemp = GLHEInletTemp;
                        WrapperReport(WrapperNum).GLHEOutletTemp = GLHEOutletTemp;
                        WrapperReport(WrapperNum).CHWmdot = CHWInletMassFlowRate;
                        WrapperReport(WrapperNum).HWmdot = HWInletMassFlowRate;
                        WrapperReport(WrapperNum).GLHEmdot = GLHEInletMassFlowRate;
                        WrapperReport(WrapperNum).TotElecCooling = WrapperElecEnergyCool;
                        WrapperReport(WrapperNum).TotElecHeating = WrapperElecEnergyHeat;
                        WrapperReport(WrapperNum).CoolingEnergy = WrapperCoolEnergy;
                        WrapperReport(WrapperNum).HeatingEnergy = WrapperHeatEnergy;
                        WrapperReport(WrapperNum).GLHEEnergy = WrapperGLHEEnergy;
                        WrapperReport(WrapperNum).TotElecCoolingPwr = WrapperElecPowerCool;
                        WrapperReport(WrapperNum).TotElecHeatingPwr = WrapperElecPowerHeat;
                        WrapperReport(WrapperNum).CoolingRate = WrapperCoolRate;
                        WrapperReport(WrapperNum).HeatingRate = WrapperHeatRate;
                        WrapperReport(WrapperNum).GLHERate = WrapperGLHERate;

                        PlantUtilities::SetComponentFlowRate(CHWInletMassFlowRate,
                                                             Wrapper(WrapperNum).CHWInletNodeNum,
                                                             Wrapper(WrapperNum).CHWOutletNodeNum,
                                             Wrapper(WrapperNum).CWLoopNum,
                                             Wrapper(WrapperNum).CWLoopSideNum,
                                             Wrapper(WrapperNum).CWBranchNum,
                                             Wrapper(WrapperNum).CWCompNum);

                        PlantUtilities::SetComponentFlowRate(HWInletMassFlowRate,
                                                             Wrapper(WrapperNum).HWInletNodeNum,
                                                             Wrapper(WrapperNum).HWOutletNodeNum,
                                             Wrapper(WrapperNum).HWLoopNum,
                                             Wrapper(WrapperNum).HWLoopSideNum,
                                             Wrapper(WrapperNum).HWBranchNum,
                                             Wrapper(WrapperNum).HWCompNum);

                        PlantUtilities::SetComponentFlowRate(GLHEInletMassFlowRate,
                                                             Wrapper(WrapperNum).GLHEInletNodeNum,
                                                             Wrapper(WrapperNum).GLHEOutletNodeNum,
                                             Wrapper(WrapperNum).GLHELoopNum,
                                             Wrapper(WrapperNum).GLHELoopSideNum,
                                             Wrapper(WrapperNum).GLHEBranchNum,
                                             Wrapper(WrapperNum).GLHECompNum);
                    }

                } // Heating loop calculation
            }
        }
    }

    void UpdateChillerRecords(int const WrapperNum) // Wrapper number
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Daeho Kang, PNNL
        //       DATE WRITTEN:    Feb 2013

        // PURPOSE OF THIS SUBROUTINE:
        //  Update chiller heater variables

        Real64 SecInTimeStep; // Number of seconds per HVAC system time step, to convert from W (J/s) to J
        int ChillerHeaterNum; // Chiller heater number

        SecInTimeStep = DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;

        for (ChillerHeaterNum = 1; ChillerHeaterNum <= Wrapper(WrapperNum).ChillerHeaterNums; ++ChillerHeaterNum) {
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoad =
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoadRate * SecInTimeStep;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingEnergy =
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPower * SecInTimeStep;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingEnergy =
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingPower * SecInTimeStep;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergy =
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvap * SecInTimeStep;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergy =
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCond * SecInTimeStep;
            if (Wrapper(WrapperNum).SimulClgDominant || Wrapper(WrapperNum).SimulHtgDominant) {
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoadSimul =
                    Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoad;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingEnergySimul =
                    Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingEnergy;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergySimul =
                    Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergy;
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergySimul =
                    Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergy;
            }
        }
    }

    void UpdateChillerHeaterRecords(int const WrapperNum) // Wrapper number
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Daeho Kang, PNNL
        //       DATE WRITTEN:    Feb 2013

        // Number of seconds per HVAC system time step, to convert from W (J/s) to J
        Real64 SecInTimeStep = DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;

        for (int ChillerHeaterNum = 1; ChillerHeaterNum <= Wrapper(WrapperNum).ChillerHeaterNums; ++ChillerHeaterNum) {
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoad =
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).ChillerFalseLoadRate * SecInTimeStep;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingEnergy =
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CoolingPower * SecInTimeStep;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingEnergy =
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).HeatingPower * SecInTimeStep;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).EvapEnergy =
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QEvap * SecInTimeStep;
            Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).CondEnergy =
                Wrapper(WrapperNum).ChillerHeaterReport(ChillerHeaterNum).QCond * SecInTimeStep;
        }
    }

} // namespace PlantCentralGSHP

} // namespace EnergyPlus
