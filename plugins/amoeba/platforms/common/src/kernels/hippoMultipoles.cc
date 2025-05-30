KERNEL void computeLabFrameMoments(GLOBAL real4* RESTRICT posq, GLOBAL int4* RESTRICT multipoleParticles, GLOBAL real* RESTRICT localDipoles,
        GLOBAL real* RESTRICT localQuadrupoles, GLOBAL real* RESTRICT labDipoles, GLOBAL real* RESTRICT labQXX, GLOBAL real* RESTRICT labQXY, GLOBAL real* RESTRICT labQXZ,
        GLOBAL real* RESTRICT labQYY, GLOBAL real* RESTRICT labQYZ
#ifdef USE_PERIODIC
        ,real4 periodicBoxSize, real4 invPeriodicBoxSize, real4 periodicBoxVecX, real4 periodicBoxVecY, real4 periodicBoxVecZ
#endif
         ) {
    for (int atom = GLOBAL_ID; atom < NUM_ATOMS; atom += GLOBAL_SIZE) {
        int4 particles = multipoleParticles[atom];
        if (particles.z >= 0) {
            real4 thisParticlePos = posq[atom];
            real4 posZ = posq[particles.z];
            real3 vectorZ = make_real3(posZ.x-thisParticlePos.x, posZ.y-thisParticlePos.y, posZ.z-thisParticlePos.z);
#ifdef USE_PERIODIC
            APPLY_PERIODIC_TO_DELTA(vectorZ)
#endif
            vectorZ = normalize(vectorZ);
            int axisType = particles.w; 
            real4 posX;
            real3 vectorX;
            if (axisType >= 4) {
                if (fabs(vectorZ.x) < 0.866)
                    vectorX = make_real3(1, 0, 0);
                else
                    vectorX = make_real3(0, 1, 0);
            }
            else {
                posX = posq[particles.x];
                vectorX = make_real3(posX.x-thisParticlePos.x, posX.y-thisParticlePos.y, posX.z-thisParticlePos.z);
#ifdef USE_PERIODIC
                APPLY_PERIODIC_TO_DELTA(vectorX)
#endif
            }
        
            // branch based on axis type
                    
            if (axisType == 1) {
        
                // bisector
                
                vectorX = normalize(vectorX);
                vectorZ += vectorX;
                vectorZ = normalize(vectorZ);
            }
            else if (axisType == 2 || axisType == 3) { 
         
                // z-bisect
        
                if (particles.y >= 0 && particles.y < NUM_ATOMS) {
                    real4 posY = posq[particles.y];
                    real3 vectorY = make_real3(posY.x-thisParticlePos.x, posY.y-thisParticlePos.y, posY.z-thisParticlePos.z);
#ifdef USE_PERIODIC
                    APPLY_PERIODIC_TO_DELTA(vectorY)
#endif
                    vectorY = normalize(vectorY);
                    vectorX = normalize(vectorX);
                    if (axisType == 2) {
                        vectorX += vectorY;
                        vectorX = normalize(vectorX);
                    }
                    else { 
             
                        // 3-fold
                
                        vectorZ += vectorX + vectorY;
                        vectorZ = normalize(vectorZ);
                    }
                }
         
            }
            
            // x = x - (x.z)z
        
            vectorX -= dot(vectorZ, vectorX)*vectorZ;
            vectorX = normalize(vectorX);
            real3 vectorY = cross(vectorZ, vectorX);
         
            // use identity rotation matrix for unrecognized axis types
        
            if (axisType < 0 || axisType > 4) {
        
                vectorX.x = 1;
                vectorX.y = 0;
                vectorX.z = 0;
        
                vectorY.x = 0;
                vectorY.y = 1;
                vectorY.z = 0;
        
                vectorZ.x = 0;
                vectorZ.y = 0;
                vectorZ.z = 1;
            }
            
            // Check the chirality and see whether it needs to be reversed
            
            bool reverse = false;
            if (axisType == 0 && particles.x >= 0 && particles.y >=0 && particles.z >= 0) {
                real4 posY = posq[particles.y];
                real3 delta[4];

                delta[0].x = thisParticlePos.x - posY.x;
                delta[0].y = thisParticlePos.y - posY.y;
                delta[0].z = thisParticlePos.z - posY.z;
#ifdef USE_PERIODIC
                APPLY_PERIODIC_TO_DELTA(delta[0])
#endif

                delta[1].x = posZ.x - posY.x;
                delta[1].y = posZ.y - posY.y;
                delta[1].z = posZ.z - posY.z;
#ifdef USE_PERIODIC
                APPLY_PERIODIC_TO_DELTA(delta[1])
#endif

                delta[2].x = posX.x - posY.x;
                delta[2].y = posX.y - posY.y;
                delta[2].z = posX.z - posY.z;
#ifdef USE_PERIODIC
                APPLY_PERIODIC_TO_DELTA(delta[2])
#endif

                delta[3].x = delta[1].y*delta[2].z - delta[1].z*delta[2].y;
                delta[3].y = delta[2].y*delta[0].z - delta[2].z*delta[0].y;
                delta[3].z = delta[0].y*delta[1].z - delta[0].z*delta[1].y;
#ifdef USE_PERIODIC
                APPLY_PERIODIC_TO_DELTA(delta[3])
#endif

                real volume = delta[3].x*delta[0].x + delta[3].y*delta[1].x + delta[3].z*delta[2].x;
                reverse = (volume < 0);
            }
        
            // Transform the dipole
            
            real3 dipole = make_real3(localDipoles[3*atom], localDipoles[3*atom+1], localDipoles[3*atom+2]);
            if (reverse)
                dipole.y *= -1;
            labDipoles[3*atom] = dipole.x*vectorX.x + dipole.y*vectorY.x + dipole.z*vectorZ.x;
            labDipoles[3*atom+1] = dipole.x*vectorX.y + dipole.y*vectorY.y + dipole.z*vectorZ.y;
            labDipoles[3*atom+2] = dipole.x*vectorX.z + dipole.y*vectorY.z + dipole.z*vectorZ.z;
            
            // Transform the quadrupole
            
            real mPoleXX = localQuadrupoles[5*atom];
            real mPoleXY = localQuadrupoles[5*atom+1];
            real mPoleXZ = localQuadrupoles[5*atom+2];
            real mPoleYY = localQuadrupoles[5*atom+3];
            real mPoleYZ = localQuadrupoles[5*atom+4];
            real mPoleZZ = -(mPoleXX+mPoleYY);
        
            if (reverse) {
                mPoleXY *= -1;
                mPoleYZ *= -1;
            }
            
            labQXX[atom] = vectorX.x*(vectorX.x*mPoleXX + vectorY.x*mPoleXY + vectorZ.x*mPoleXZ)
                         + vectorY.x*(vectorX.x*mPoleXY + vectorY.x*mPoleYY + vectorZ.x*mPoleYZ)
                         + vectorZ.x*(vectorX.x*mPoleXZ + vectorY.x*mPoleYZ + vectorZ.x*mPoleZZ);
            labQXY[atom] = vectorX.x*(vectorX.y*mPoleXX + vectorY.y*mPoleXY + vectorZ.y*mPoleXZ)
                         + vectorY.x*(vectorX.y*mPoleXY + vectorY.y*mPoleYY + vectorZ.y*mPoleYZ)
                         + vectorZ.x*(vectorX.y*mPoleXZ + vectorY.y*mPoleYZ + vectorZ.y*mPoleZZ);
            labQXZ[atom] = vectorX.x*(vectorX.z*mPoleXX + vectorY.z*mPoleXY + vectorZ.z*mPoleXZ)
                         + vectorY.x*(vectorX.z*mPoleXY + vectorY.z*mPoleYY + vectorZ.z*mPoleYZ)
                         + vectorZ.x*(vectorX.z*mPoleXZ + vectorY.z*mPoleYZ + vectorZ.z*mPoleZZ);
            labQYY[atom] = vectorX.y*(vectorX.y*mPoleXX + vectorY.y*mPoleXY + vectorZ.y*mPoleXZ)
                         + vectorY.y*(vectorX.y*mPoleXY + vectorY.y*mPoleYY + vectorZ.y*mPoleYZ)
                         + vectorZ.y*(vectorX.y*mPoleXZ + vectorY.y*mPoleYZ + vectorZ.y*mPoleZZ);
            labQYZ[atom] = vectorX.y*(vectorX.z*mPoleXX + vectorY.z*mPoleXY + vectorZ.z*mPoleXZ)
                         + vectorY.y*(vectorX.z*mPoleXY + vectorY.z*mPoleYY + vectorZ.z*mPoleYZ)
                         + vectorZ.y*(vectorX.z*mPoleXZ + vectorY.z*mPoleYZ + vectorZ.z*mPoleZZ);
        }
        else {
            labDipoles[3*atom] = localDipoles[3*atom];
            labDipoles[3*atom+1] = localDipoles[3*atom+1];
            labDipoles[3*atom+2] = localDipoles[3*atom+2];
            labQXX[atom] = localQuadrupoles[5*atom];
            labQXY[atom] = localQuadrupoles[5*atom+1];
            labQXZ[atom] = localQuadrupoles[5*atom+2];
            labQYY[atom] = localQuadrupoles[5*atom+3];
            labQYZ[atom] = localQuadrupoles[5*atom+4];
        }
    }
}

KERNEL void recordInducedDipoles(GLOBAL const mm_long* RESTRICT fieldBuffers, GLOBAL real* RESTRICT inducedDipole, GLOBAL const real* RESTRICT polarizability) {
    for (int atom = GLOBAL_ID; atom < NUM_ATOMS; atom += GLOBAL_SIZE) {
        real scale = polarizability[atom]/(real) 0x100000000;
        inducedDipole[3*atom] = scale*fieldBuffers[atom];
        inducedDipole[3*atom+1] = scale*fieldBuffers[atom+PADDED_NUM_ATOMS];
        inducedDipole[3*atom+2] = scale*fieldBuffers[atom+PADDED_NUM_ATOMS*2];
    }
}

/**
 * Normalize a vector and return what its magnitude was.
 */
inline DEVICE real normVector(real3* v) {
    real n = SQRT(dot(*v, *v));
    *v *= (n > 0 ? RECIP(n) : 0);
    return n;
}

/**
 * Compute the force on each particle due to the torque.
 */
KERNEL void mapTorqueToForce(GLOBAL mm_ulong* RESTRICT forceBuffers, GLOBAL const mm_long* RESTRICT torqueBuffers,
        GLOBAL const real4* RESTRICT posq, GLOBAL const int4* RESTRICT multipoleParticles
#ifdef USE_PERIODIC
        ,real4 periodicBoxSize, real4 invPeriodicBoxSize, real4 periodicBoxVecX, real4 periodicBoxVecY, real4 periodicBoxVecZ
#endif
         ) {
    const int U = 0;
    const int V = 1;
    const int W = 2;
    const int R = 3;
    const int S = 4;
    const int UV = 5;
    const int UW = 6;
    const int VW = 7;
    const int UR = 8;
    const int US = 9;
    const int VS = 10;
    const int WS = 11;
    
    const int X = 0;
    const int Y = 1;
    const int Z = 2;
    const int I = 3;
    
    const real torqueScale = RECIP((double) 0x100000000);
    
    real3 forces[4];
    real norms[12];
    real3 vector[12];
    real angles[12][2];
  
    for (int atom = GLOBAL_ID; atom < NUM_ATOMS; atom += GLOBAL_SIZE) {
        int4 particles = multipoleParticles[atom];
        int axisAtom = particles.z;
        int axisType = particles.w;
    
        // NoAxisType
    
        if (axisType < 5 && particles.z >= 0) {
            real3 atomPos = trimTo3(posq[atom]);
            vector[U] = atomPos - trimTo3(posq[axisAtom]);
#ifdef USE_PERIODIC
            APPLY_PERIODIC_TO_DELTA(vector[U])
#endif
            norms[U] = normVector(&vector[U]);
            if (axisType != 4 && particles.x >= 0) {
                vector[V] = atomPos - trimTo3(posq[particles.x]);
#ifdef USE_PERIODIC
                APPLY_PERIODIC_TO_DELTA(vector[V])
#endif
            }
            else {
                if (fabs(vector[U].x/norms[U]) < 0.866)
                    vector[V] = make_real3(1, 0, 0);
                else
                    vector[V] = make_real3(0, 1, 0);
            }
            norms[V] = normVector(&vector[V]);
        
            // W = UxV
        
            if (axisType < 2 || axisType > 3)
                vector[W] = cross(vector[U], vector[V]);
            else
                vector[W] = trimTo3(posq[particles.y]) - atomPos;
            norms[W] = normVector(&vector[W]);
        
            vector[UV] = cross(vector[V], vector[U]);
            vector[UW] = cross(vector[W], vector[U]);
            vector[VW] = cross(vector[W], vector[V]);
        
            norms[UV] = normVector(&vector[UV]);
            norms[UW] = normVector(&vector[UW]);
            norms[VW] = normVector(&vector[VW]);
        
            angles[UV][0] = dot(vector[U], vector[V]);
            angles[UV][1] = SQRT(1 - angles[UV][0]*angles[UV][0]);
        
            angles[UW][0] = dot(vector[U], vector[W]);
            angles[UW][1] = SQRT(1 - angles[UW][0]*angles[UW][0]);
        
            angles[VW][0] = dot(vector[V], vector[W]);
            angles[VW][1] = SQRT(1 - angles[VW][0]*angles[VW][0]);
        
            real dphi[3];
            real3 torque = make_real3(torqueScale*torqueBuffers[atom], torqueScale*torqueBuffers[atom+PADDED_NUM_ATOMS], torqueScale*torqueBuffers[atom+PADDED_NUM_ATOMS*2]);
            dphi[U] = -dot(vector[U], torque);
            dphi[V] = -dot(vector[V], torque);
            dphi[W] = -dot(vector[W], torque);
        
            // z-then-x and bisector
        
            if (axisType == 0 || axisType == 1) {
                real factor1 = dphi[V]/(norms[U]*angles[UV][1]);
                real factor2 = dphi[W]/(norms[U]);
                real factor3 = -dphi[U]/(norms[V]*angles[UV][1]);
                real factor4 = 0;
                if (axisType == 1) {
                    factor2 *= 0.5f;
                    factor4 = 0.5f*dphi[W]/(norms[V]);
                }
                forces[Z] = vector[UV]*factor1 + factor2*vector[UW];
                forces[X] = vector[UV]*factor3 + factor4*vector[VW];
                forces[I] = -(forces[X]+forces[Z]);
                forces[Y] = make_real3(0);
            }
            else if (axisType == 2) {
                // z-bisect
        
                vector[R] = vector[V] + vector[W]; 
        
                vector[S] = cross(vector[U], vector[R]);
        
                norms[R] = normVector(&vector[R]);
                norms[S] = normVector(&vector[S]);
        
                vector[UR] = cross(vector[R], vector[U]);
                vector[US] = cross(vector[S], vector[U]);
                vector[VS] = cross(vector[S], vector[V]);
                vector[WS] = cross(vector[S], vector[W]);
        
                norms[UR] = normVector(&vector[UR]);
                norms[US] = normVector(&vector[US]);
                norms[VS] = normVector(&vector[VS]);
                norms[WS] = normVector(&vector[WS]);
        
                angles[UR][0] = dot(vector[U], vector[R]);
                angles[UR][1] = SQRT(1 - angles[UR][0]*angles[UR][0]);
        
                angles[US][0] = dot(vector[U], vector[S]);
                angles[US][1] = SQRT(1 - angles[US][0]*angles[US][0]);
        
                angles[VS][0] = dot(vector[V], vector[S]);
                angles[VS][1] = SQRT(1 - angles[VS][0]*angles[VS][0]);
        
                angles[WS][0] = dot(vector[W], vector[S]);
                angles[WS][1] = SQRT(1 - angles[WS][0]*angles[WS][0]);
         
                real3 t1 = vector[V] - vector[S]*angles[VS][0];
                real3 t2 = vector[W] - vector[S]*angles[WS][0];
                normVector(&t1);
                normVector(&t2);
                real ut1cos = dot(vector[U], t1);
                real ut1sin = SQRT(1 - ut1cos*ut1cos);
                real ut2cos = dot(vector[U], t2);
                real ut2sin = SQRT(1 - ut2cos*ut2cos);
        
                real dphiR = -dot(vector[R], torque);
                real dphiS = -dot(vector[S], torque);
        
                real factor1 = dphiR/(norms[U]*angles[UR][1]);
                real factor2 = dphiS/(norms[U]);
                real factor3 = dphi[U]/(norms[V]*(ut1sin+ut2sin));
                real factor4 = dphi[U]/(norms[W]*(ut1sin+ut2sin));
                forces[Z] = vector[UR]*factor1 + factor2*vector[US];
                forces[X] = (angles[VS][1]*vector[S] - angles[VS][0]*t1)*factor3;
                forces[Y] = (angles[WS][1]*vector[S] - angles[WS][0]*t2)*factor4;
                forces[I] = -(forces[X] + forces[Y] + forces[Z]);
            }
            else if (axisType == 3) {
                // 3-fold
        
                forces[Z] = (vector[UW]*dphi[W]/(norms[U]*angles[UW][1]) +
                            vector[UV]*dphi[V]/(norms[U]*angles[UV][1]) -
                            vector[UW]*dphi[U]/(norms[U]*angles[UW][1]) -
                            vector[UV]*dphi[U]/(norms[U]*angles[UV][1]))/3;

                forces[X] = (vector[VW]*dphi[W]/(norms[V]*angles[VW][1]) -
                            vector[UV]*dphi[U]/(norms[V]*angles[UV][1]) -
                            vector[VW]*dphi[V]/(norms[V]*angles[VW][1]) +
                            vector[UV]*dphi[V]/(norms[V]*angles[UV][1]))/3;

                forces[Y] = (-vector[UW]*dphi[U]/(norms[W]*angles[UW][1]) -
                            vector[VW]*dphi[V]/(norms[W]*angles[VW][1]) +
                            vector[UW]*dphi[W]/(norms[W]*angles[UW][1]) +
                            vector[VW]*dphi[W]/(norms[W]*angles[VW][1]))/3;
                forces[I] = -(forces[X] + forces[Y] + forces[Z]);
            }
            else if (axisType == 4) {
                // z-only
        
                forces[Z] = vector[UV]*dphi[V]/(norms[U]*angles[UV][1]) + vector[UW]*dphi[W]/norms[U];
                forces[X] = make_real3(0);
                forces[Y] = make_real3(0);
                forces[I] = -forces[Z];
            }
            else {
                forces[Z] = make_real3(0);
                forces[X] = make_real3(0);
                forces[Y] = make_real3(0);
                forces[I] = make_real3(0);
            }

            // Store results

            ATOMIC_ADD(&forceBuffers[particles.z], (mm_ulong) realToFixedPoint(forces[Z].x));
            ATOMIC_ADD(&forceBuffers[particles.z+PADDED_NUM_ATOMS], (mm_ulong) realToFixedPoint(forces[Z].y));
            ATOMIC_ADD(&forceBuffers[particles.z+2*PADDED_NUM_ATOMS], (mm_ulong) realToFixedPoint(forces[Z].z));
            if (axisType != 4) {
                ATOMIC_ADD(&forceBuffers[particles.x], (mm_ulong) realToFixedPoint(forces[X].x));
                ATOMIC_ADD(&forceBuffers[particles.x+PADDED_NUM_ATOMS], (mm_ulong) realToFixedPoint(forces[X].y));
                ATOMIC_ADD(&forceBuffers[particles.x+2*PADDED_NUM_ATOMS], (mm_ulong) realToFixedPoint(forces[X].z));
            }
            if ((axisType == 2 || axisType == 3) && particles.y > -1) {
                ATOMIC_ADD(&forceBuffers[particles.y], (mm_ulong) realToFixedPoint(forces[Y].x));
                ATOMIC_ADD(&forceBuffers[particles.y+PADDED_NUM_ATOMS], (mm_ulong) realToFixedPoint(forces[Y].y));
                ATOMIC_ADD(&forceBuffers[particles.y+2*PADDED_NUM_ATOMS], (mm_ulong) realToFixedPoint(forces[Y].z));
            }
            ATOMIC_ADD(&forceBuffers[atom], (mm_ulong) realToFixedPoint(forces[I].x));
            ATOMIC_ADD(&forceBuffers[atom+PADDED_NUM_ATOMS], (mm_ulong) realToFixedPoint(forces[I].y));
            ATOMIC_ADD(&forceBuffers[atom+2*PADDED_NUM_ATOMS], (mm_ulong) realToFixedPoint(forces[I].z));
        }
    }
}
