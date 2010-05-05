
/* Portions copyright (c) 2009 Stanford University and Simbios.
 * Contributors: Peter Eastman
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <sstream>

#include "../SimTKUtilities/SimTKOpenMMCommon.h"
#include "../SimTKUtilities/SimTKOpenMMLog.h"
#include "../SimTKUtilities/SimTKOpenMMUtilities.h"
#include "ReferenceForce.h"
#include "ReferenceCustomGBIxn.h"

using std::map;
using std::set;
using std::string;
using std::stringstream;
using std::vector;

/**---------------------------------------------------------------------------------------

   ReferenceCustomGBIxn constructor

   --------------------------------------------------------------------------------------- */

ReferenceCustomGBIxn::ReferenceCustomGBIxn(const vector<Lepton::ExpressionProgram>& valueExpressions,
                     const vector<vector<Lepton::ExpressionProgram> > valueDerivExpressions,
                     const vector<vector<Lepton::ExpressionProgram> > valueGradientExpressions,
                     const vector<string>& valueNames,
                     const vector<OpenMM::CustomGBForce::ComputationType>& valueTypes,
                     const vector<Lepton::ExpressionProgram>& energyExpressions,
                     const vector<vector<Lepton::ExpressionProgram> > energyDerivExpressions,
                     const vector<vector<Lepton::ExpressionProgram> > energyGradientExpressions,
                     const vector<OpenMM::CustomGBForce::ComputationType>& energyTypes,
                     const vector<string>& parameterNames) :
            cutoff(false), periodic(false), valueExpressions(valueExpressions), valueDerivExpressions(valueDerivExpressions), valueGradientExpressions(valueGradientExpressions),
            valueNames(valueNames), valueTypes(valueTypes), energyExpressions(energyExpressions), energyDerivExpressions(energyDerivExpressions), energyGradientExpressions(energyGradientExpressions),
            energyTypes(energyTypes), paramNames(parameterNames) {

   // ---------------------------------------------------------------------------------------

   // static const char* methodName = "\nReferenceCustomGBIxn::ReferenceCustomGBIxn";

   // ---------------------------------------------------------------------------------------

    for (int i = 0; i < (int) paramNames.size(); i++) {
        for (int j = 1; j < 3; j++) {
            stringstream name;
            name << paramNames[i] << j;
            particleParamNames.push_back(name.str());
        }
    }
    for (int i = 0; i < (int) valueNames.size(); i++) {
        for (int j = 1; j < 3; j++) {
            stringstream name;
            name << valueNames[i] << j;
            particleValueNames.push_back(name.str());
        }
    }
}

/**---------------------------------------------------------------------------------------

   ReferenceCustomGBIxn destructor

   --------------------------------------------------------------------------------------- */

ReferenceCustomGBIxn::~ReferenceCustomGBIxn( ){

   // ---------------------------------------------------------------------------------------

   // static const char* methodName = "\nReferenceCustomGBIxn::~ReferenceCustomGBIxn";

   // ---------------------------------------------------------------------------------------

}

  /**---------------------------------------------------------------------------------------

     Set the force to use a cutoff.

     @param distance            the cutoff distance
     @param neighbors           the neighbor list to use

     --------------------------------------------------------------------------------------- */

  void ReferenceCustomGBIxn::setUseCutoff( RealOpenMM distance, const OpenMM::NeighborList& neighbors ) {

    cutoff = true;
    cutoffDistance = distance;
    neighborList = &neighbors;
  }

  /**---------------------------------------------------------------------------------------

     Set the force to use periodic boundary conditions.  This requires that a cutoff has
     also been set, and the smallest side of the periodic box is at least twice the cutoff
     distance.

     @param boxSize             the X, Y, and Z widths of the periodic box

     --------------------------------------------------------------------------------------- */

  void ReferenceCustomGBIxn::setPeriodic( RealOpenMM* boxSize ) {

    assert(cutoff);
    assert(boxSize[0] >= 2.0*cutoffDistance);
    assert(boxSize[1] >= 2.0*cutoffDistance);
    assert(boxSize[2] >= 2.0*cutoffDistance);
    periodic = true;
    periodicBoxSize[0] = boxSize[0];
    periodicBoxSize[1] = boxSize[1];
    periodicBoxSize[2] = boxSize[2];
  }

void ReferenceCustomGBIxn::calculateIxn(int numberOfAtoms, RealOpenMM** atomCoordinates, RealOpenMM** atomParameters,
                                           const vector<set<int> >& exclusions, map<string, double>& globalParameters, RealOpenMM** forces,
                                           RealOpenMM* totalEnergy) const {
    // First calculate the computed values.

    int numValues = valueTypes.size();
    vector<vector<RealOpenMM> > values(numValues);
    for (int valueIndex = 0; valueIndex < numValues; valueIndex++) {
        if (valueTypes[valueIndex] == OpenMM::CustomGBForce::SingleParticle)
            calculateSingleParticleValue(valueIndex, numberOfAtoms, atomCoordinates, values, globalParameters, atomParameters);
        else if (valueTypes[valueIndex] == OpenMM::CustomGBForce::ParticlePair)
            calculateParticlePairValue(valueIndex, numberOfAtoms, atomCoordinates, atomParameters, values, globalParameters, exclusions, true);
        else
            calculateParticlePairValue(valueIndex, numberOfAtoms, atomCoordinates, atomParameters, values, globalParameters, exclusions, false);
    }

    // Now calculate the energy and its derivates.

    vector<vector<RealOpenMM> > dEdV(numValues, vector<RealOpenMM>(numberOfAtoms, (RealOpenMM) 0));
    for (int termIndex = 0; termIndex < (int) energyExpressions.size(); termIndex++) {
        if (energyTypes[termIndex] == OpenMM::CustomGBForce::SingleParticle)
            calculateSingleParticleEnergyTerm(termIndex, numberOfAtoms, atomCoordinates, values, globalParameters, atomParameters, forces, totalEnergy, dEdV);
        else if (energyTypes[termIndex] == OpenMM::CustomGBForce::ParticlePair)
            calculateParticlePairEnergyTerm(termIndex, numberOfAtoms, atomCoordinates, atomParameters, values, globalParameters, exclusions, true, forces, totalEnergy, dEdV);
        else
            calculateParticlePairEnergyTerm(termIndex, numberOfAtoms, atomCoordinates, atomParameters, values, globalParameters, exclusions, false, forces, totalEnergy, dEdV);
    }

    // Apply the chain rule to evaluate forces.

    calculateChainRuleForces(numberOfAtoms, atomCoordinates, atomParameters, values, globalParameters, exclusions, forces, dEdV);
}

void ReferenceCustomGBIxn::calculateSingleParticleValue(int index, int numAtoms, RealOpenMM** atomCoordinates, vector<vector<RealOpenMM> >& values,
        const map<string, double>& globalParameters, RealOpenMM** atomParameters) const {
    values[index].resize(numAtoms);
    map<string, double> variables = globalParameters;
    for (int i = 0; i < numAtoms; i++) {
        variables["x"] = atomCoordinates[i][0];
        variables["y"] = atomCoordinates[i][1];
        variables["z"] = atomCoordinates[i][2];
        for (int j = 0; j < (int) paramNames.size(); j++)
            variables[paramNames[j]] = atomParameters[i][j];
        for (int j = 0; j < index; j++)
            variables[valueNames[j]] = values[j][i];
        values[index][i] = (RealOpenMM) valueExpressions[index].evaluate(variables);
    }
}

void ReferenceCustomGBIxn::calculateParticlePairValue(int index, int numAtoms, RealOpenMM** atomCoordinates, RealOpenMM** atomParameters,
        vector<vector<RealOpenMM> >& values, const map<string, double>& globalParameters, const vector<set<int> >& exclusions, bool useExclusions) const {
    values[index].resize(numAtoms);
    for (int i = 0; i < numAtoms; i++)
        values[index][i] = (RealOpenMM) 0.0;
    if (cutoff) {
        // Loop over all pairs in the neighbor list.

        for (int i = 0; i < (int) neighborList->size(); i++) {
            OpenMM::AtomPair pair = (*neighborList)[i];
            if (useExclusions && exclusions[pair.first].find(pair.second) != exclusions[pair.first].end())
                continue;
            calculateOnePairValue(index, pair.first, pair.second, atomCoordinates, atomParameters, globalParameters, values);
            calculateOnePairValue(index, pair.second, pair.first, atomCoordinates, atomParameters, globalParameters, values);
        }
    }
    else {
        // Perform an O(N^2) loop over all atom pairs.

        for (int i = 0; i < numAtoms; i++){
            for (int j = i+1; j < numAtoms; j++ ){
                if (useExclusions && exclusions[i].find(j) != exclusions[i].end())
                    continue;
                calculateOnePairValue(index, i, j, atomCoordinates, atomParameters, globalParameters, values);
                calculateOnePairValue(index, j, i, atomCoordinates, atomParameters, globalParameters, values);
           }
        }
    }
}

void ReferenceCustomGBIxn::calculateOnePairValue(int index, int atom1, int atom2, RealOpenMM** atomCoordinates, RealOpenMM** atomParameters,
        const map<string, double>& globalParameters, vector<vector<RealOpenMM> >& values) const {
    RealOpenMM deltaR[ReferenceForce::LastDeltaRIndex];
    if (periodic)
        ReferenceForce::getDeltaRPeriodic(atomCoordinates[atom2], atomCoordinates[atom1], periodicBoxSize, deltaR);
    else
        ReferenceForce::getDeltaR(atomCoordinates[atom2], atomCoordinates[atom1], deltaR);
    RealOpenMM r = deltaR[ReferenceForce::RIndex];
    if (cutoff && r >= cutoffDistance)
        return;
    map<string, double> variables = globalParameters;
    for (int i = 0; i < (int) paramNames.size(); i++) {
        variables[particleParamNames[i*2]] = atomParameters[atom1][i];
        variables[particleParamNames[i*2+1]] = atomParameters[atom2][i];
    }
    variables["r"] = r;
    for (int i = 0; i < index; i++) {
        variables[particleValueNames[i*2]] = values[i][atom1];
        variables[particleValueNames[i*2+1]] = values[i][atom2];
    }
    values[index][atom1] += (RealOpenMM) valueExpressions[index].evaluate(variables);
}

void ReferenceCustomGBIxn::calculateSingleParticleEnergyTerm(int index, int numAtoms, RealOpenMM** atomCoordinates, const vector<vector<RealOpenMM> >& values,
        const map<string, double>& globalParameters, RealOpenMM** atomParameters, RealOpenMM** forces, RealOpenMM* totalEnergy,
        vector<vector<RealOpenMM> >& dEdV) const {
    map<string, double> variables = globalParameters;
    for (int i = 0; i < numAtoms; i++) {
        variables["x"] = atomCoordinates[i][0];
        variables["y"] = atomCoordinates[i][1];
        variables["z"] = atomCoordinates[i][2];
        for (int j = 0; j < (int) paramNames.size(); j++)
            variables[paramNames[j]] = atomParameters[i][j];
        for (int j = 0; j < (int) valueNames.size(); j++)
            variables[valueNames[j]] = values[j][i];
        if (totalEnergy != NULL)
            *totalEnergy += (RealOpenMM) energyExpressions[index].evaluate(variables);
        for (int j = 0; j < (int) valueNames.size(); j++)
            dEdV[j][i] += (RealOpenMM) energyDerivExpressions[index][j].evaluate(variables);
        forces[i][0] -= (RealOpenMM) energyGradientExpressions[index][0].evaluate(variables);
        forces[i][1] -= (RealOpenMM) energyGradientExpressions[index][1].evaluate(variables);
        forces[i][2] -= (RealOpenMM) energyGradientExpressions[index][2].evaluate(variables);
    }
}

void ReferenceCustomGBIxn::calculateParticlePairEnergyTerm(int index, int numAtoms, RealOpenMM** atomCoordinates, RealOpenMM** atomParameters,
        const vector<vector<RealOpenMM> >& values, const map<string, double>& globalParameters, const vector<set<int> >& exclusions, bool useExclusions,
        RealOpenMM** forces, RealOpenMM* totalEnergy, vector<vector<RealOpenMM> >& dEdV) const {
    if (cutoff) {
        // Loop over all pairs in the neighbor list.

        for (int i = 0; i < (int) neighborList->size(); i++) {
            OpenMM::AtomPair pair = (*neighborList)[i];
            if (useExclusions && exclusions[pair.first].find(pair.second) != exclusions[pair.first].end())
                continue;
            calculateOnePairEnergyTerm(index, pair.first, pair.second, atomCoordinates, atomParameters, globalParameters, values, forces, totalEnergy, dEdV);
        }
    }
    else {
        // Perform an O(N^2) loop over all atom pairs.

        for (int i = 0; i < numAtoms; i++){
            for (int j = i+1; j < numAtoms; j++ ){
                if (useExclusions && exclusions[i].find(j) != exclusions[i].end())
                    continue;
                calculateOnePairEnergyTerm(index, i, j, atomCoordinates, atomParameters, globalParameters, values, forces, totalEnergy, dEdV);
           }
        }
    }
}

void ReferenceCustomGBIxn::calculateOnePairEnergyTerm(int index, int atom1, int atom2, RealOpenMM** atomCoordinates, RealOpenMM** atomParameters,
        const map<string, double>& globalParameters, const vector<vector<RealOpenMM> >& values, RealOpenMM** forces, RealOpenMM* totalEnergy,
        vector<vector<RealOpenMM> >& dEdV) const {
    // Compute the displacement.

    RealOpenMM deltaR[ReferenceForce::LastDeltaRIndex];
    if (periodic)
        ReferenceForce::getDeltaRPeriodic(atomCoordinates[atom2], atomCoordinates[atom1], periodicBoxSize, deltaR);
    else
        ReferenceForce::getDeltaR(atomCoordinates[atom2], atomCoordinates[atom1], deltaR);
    RealOpenMM r = deltaR[ReferenceForce::RIndex];
    if (cutoff && r >= cutoffDistance)
        return;

    // Record variables for evaluating expressions.

    map<string, double> variables = globalParameters;
    for (int i = 0; i < (int) paramNames.size(); i++) {
        variables[particleParamNames[i*2]] = atomParameters[atom1][i];
        variables[particleParamNames[i*2+1]] = atomParameters[atom2][i];
    }
    variables["r"] = r;
    for (int i = 0; i < (int) valueNames.size(); i++) {
        variables[particleValueNames[i*2]] = values[i][atom1];
        variables[particleValueNames[i*2+1]] = values[i][atom2];
    }

    // Evaluate the energy and its derivatives.

    if (totalEnergy != NULL)
        *totalEnergy += (RealOpenMM) energyExpressions[index].evaluate(variables);
    RealOpenMM dEdR = (RealOpenMM) energyDerivExpressions[index][0].evaluate(variables);
    dEdR *= 1/r;
    for (int i = 0; i < 3; i++) {
       forces[atom1][i] -= dEdR*deltaR[i];
       forces[atom2][i] += dEdR*deltaR[i];
    }
    for (int i = 0; i < (int) valueNames.size(); i++) {
        dEdV[i][atom1] += (RealOpenMM) energyDerivExpressions[index][2*i+1].evaluate(variables);
        dEdV[i][atom2] += (RealOpenMM) energyDerivExpressions[index][2*i+2].evaluate(variables);
    }
}

void ReferenceCustomGBIxn::calculateChainRuleForces(int numAtoms, RealOpenMM** atomCoordinates, RealOpenMM** atomParameters,
        const vector<vector<RealOpenMM> >& values, const map<string, double>& globalParameters,
        const vector<set<int> >& exclusions, RealOpenMM** forces, vector<vector<RealOpenMM> >& dEdV) const {
    bool useExclusions = (energyTypes[0] == OpenMM::CustomGBForce::ParticlePair);
    if (cutoff) {
        // Loop over all pairs in the neighbor list.

        for (int i = 0; i < (int) neighborList->size(); i++) {
            OpenMM::AtomPair pair = (*neighborList)[i];
            if (useExclusions && exclusions[pair.first].find(pair.second) != exclusions[pair.first].end())
                continue;
            calculateOnePairChainRule(pair.first, pair.second, atomCoordinates, atomParameters, globalParameters, values, forces, dEdV);
            calculateOnePairChainRule(pair.second, pair.first, atomCoordinates, atomParameters, globalParameters, values, forces, dEdV);
        }
    }
    else {
        // Perform an O(N^2) loop over all atom pairs.

        for (int i = 0; i < numAtoms; i++){
            for (int j = i+1; j < numAtoms; j++ ){
                if (useExclusions && exclusions[i].find(j) != exclusions[i].end())
                    continue;
                calculateOnePairChainRule(i, j, atomCoordinates, atomParameters, globalParameters, values, forces, dEdV);
                calculateOnePairChainRule(j, i, atomCoordinates, atomParameters, globalParameters, values, forces, dEdV);
           }
        }
    }
}

void ReferenceCustomGBIxn::calculateOnePairChainRule(int atom1, int atom2, RealOpenMM** atomCoordinates, RealOpenMM** atomParameters,
        const map<string, double>& globalParameters, const vector<vector<RealOpenMM> >& values, RealOpenMM** forces,
        vector<vector<RealOpenMM> >& dEdV) const {
    // Compute the displacement.

    RealOpenMM deltaR[ReferenceForce::LastDeltaRIndex];
    if (periodic)
        ReferenceForce::getDeltaRPeriodic(atomCoordinates[atom2], atomCoordinates[atom1], periodicBoxSize, deltaR);
    else
        ReferenceForce::getDeltaR(atomCoordinates[atom2], atomCoordinates[atom1], deltaR);
    RealOpenMM r = deltaR[ReferenceForce::RIndex];
    if (cutoff && r >= cutoffDistance)
        return;

    // Record variables for evaluating expressions.

    map<string, double> variables = globalParameters;
    for (int i = 0; i < (int) paramNames.size(); i++) {
        variables[particleParamNames[i*2]] = atomParameters[atom1][i];
        variables[particleParamNames[i*2+1]] = atomParameters[atom2][i];
    }
    variables["r"] = r;
    variables[particleValueNames[0]] = values[0][atom1];
    variables[particleValueNames[1]] = values[0][atom2];

    // Evaluate the derivative of each parameter with respect to position and apply forces.

    vector<vector<RealOpenMM> > gradient1(valueDerivExpressions.size(), vector<RealOpenMM>(3, 0.0));
    vector<vector<RealOpenMM> > gradient2(valueDerivExpressions.size(), vector<RealOpenMM>(3, 0.0));
    RealOpenMM dVdR = (RealOpenMM) valueDerivExpressions[0][0].evaluate(variables);
    RealOpenMM rinv = 1/r;
    for (int i = 0; i < 3; i++) {
        gradient1[0][i] = dVdR*deltaR[i]*rinv;
        gradient2[0][i] = -gradient1[0][i];
        forces[atom1][i] -= dEdV[0][atom1]*gradient1[0][i];
        forces[atom2][i] -= dEdV[0][atom1]*gradient2[0][i];
    }
    variables = globalParameters;
    for (int i = 0; i < (int) paramNames.size(); i++)
        variables[paramNames[i]] = atomParameters[atom1][i];
    variables[valueNames[0]] = values[0][atom1];
    for (int i = 1; i < (int) valueNames.size(); i++) {
        variables[valueNames[i]] = values[i][atom1];
        for (int j = 0; j < i; j++) {
            RealOpenMM dVdV = (RealOpenMM) valueDerivExpressions[i][j].evaluate(variables);
            for (int k = 0; k < 3; k++) {
                gradient1[i][k] += dVdV*gradient1[j][k];
                gradient2[i][k] += dVdV*gradient2[j][k];
            }
        }
        for (int k = 0; k < 3; k++) {
            gradient1[i][k] += (RealOpenMM) valueGradientExpressions[i][k].evaluate(variables);
            forces[atom1][k] -= dEdV[i][atom1]*gradient1[i][k];
            forces[atom2][k] -= dEdV[i][atom1]*gradient2[i][k];
        }
    }
}
