#include "Pusher.hpp"

using namespace std;
using namespace chrono;


Pusher::Pusher(shared_ptr<Loader> ldr,
               shared_ptr<GridManager> gridMnr,
               shared_ptr<BoundaryManager> boundMnr):loader(move(ldr)),
                gridMgr(move(gridMnr)), boundaryMgr(move(boundMnr)){
    
    logger.reset(new Logger());
    initialize();
    logger->writeMsg("[Pusher] create...OK", DEBUG);
}

Pusher::~Pusher(){
    delete[] particles;
}
               
void Pusher::initialize(){
    
}



void Pusher::setParticlePosition(int idx, double input[6] ){
    particles[idx]->setPosition(input);
}

void Pusher::setParticleVelocity(int idx, double input[6]){
    particles[idx]->setVelocity(input);
}

void Pusher::setParticleVelocity(int idx, int dir, double value){
    particles[idx]->setVelocity(dir, value);
}

void Pusher::setParticleType(int idx, int input){
    particles[idx]->setType(input);
}


void Pusher::initParticles(int num, int typesNum){
    
    weights = new double[typesNum];
    charges = new double[typesNum];
    masses = new double[typesNum];
    
    for(int spn=0; spn<typesNum; spn++){
        weights[spn] = 0.0;
        charges[spn] = 0.0;
        masses[spn] = 0.0;
    }
    double pos[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double vel[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    currentPartclNumOnDomain = num;
    int const ALLOCATION_FACTOR = 2;
    totalNum  = ALLOCATION_FACTOR*num;
    particles = new Particle*[totalNum];
    for(int i=0; i<totalNum; i++){
        Particle* pa = new Particle();
        particles[i] = pa;
        particles[i]->setPosition(pos);
        particles[i]->setVelocity(vel);
        particles[i]->setType(0);
    }
    int TOT_IN_BOX = 0;
    MPI_Allreduce(&currentPartclNumOnDomain, &TOT_IN_BOX, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    logger->writeMsg(("[Pusher] total particles initialized in the box = "
                                                +to_string(TOT_IN_BOX)).c_str(),  DEBUG);
    totinBoxInit = TOT_IN_BOX;
}


void Pusher::addParticles(vector<shared_ptr<Particle>> particles2add){
   
    if((currentPartclNumOnDomain+particles2add.size())>totalNum){
        auto start_time = high_resolution_clock::now();
        logger->writeMsg(("[Pusher] reinitialize ..."),  DEBUG);
        //reinitialize
        double const ALLOCATION_FACTOR = 1.5;
        int totalNumNew  = int (ALLOCATION_FACTOR*totalNum);
        
        vector<Particle*> particlesTemp;
        particlesTemp.reserve(totalNum);
        
        for(int i = 0; i < currentPartclNumOnDomain; i++){
            particlesTemp.push_back(particles[i]);
        }
        
        delete[] particles;
    
        totalNum = totalNumNew;
        particles = new Particle*[totalNum];

        for(int i=0; i<currentPartclNumOnDomain; i++){
            Particle* pa = new Particle();
            particles[i] = pa;
            particles[i]->reinitializeUsingParticle(particlesTemp[i]);
        }

        double pos[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        double vel[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        
        for(int i=currentPartclNumOnDomain; i<totalNum; i++){
            Particle* pa = new Particle();
            particles[i] = pa;
            particles[i]->setPosition(pos);
            particles[i]->setVelocity(vel);
            particles[i]->setType(0);
        }
        auto end_time = high_resolution_clock::now();
        string msg ="[Pusher] reinitialize "+to_string(totalNumNew)
                        +" particles duration = "
                            +to_string(duration_cast<milliseconds>(end_time - start_time).count())+" ms";
        logger->writeMsg(msg.c_str(),  DEBUG);
    }

    for(int i=0; i<particles2add.size(); i++){
        particles[currentPartclNumOnDomain]->reinitializeUsingParticle(particles2add[i]);
        currentPartclNumOnDomain++;
    }

}

void Pusher::setParticleWeight4Type(int type,  double weight){
    weights[type] = weight;
    logger->writeMsg(("[Pusher] weights["+to_string(type)
                                +"] = "+to_string(weight)).c_str(),  DEBUG);
}

void Pusher::setParticleCharge4Type(int type, double charge){
    charges[type] = charge;
    logger->writeMsg(("[Pusher] charges["+to_string(type)
                                +"] = "+to_string(charge)).c_str(),  DEBUG);
}

void Pusher::setParticleMass4Type(int type, double mass){
    masses[type] = mass;
    logger->writeMsg(("[Pusher] masses["+to_string(type)
                                +"] = "+to_string(mass)).c_str(),  DEBUG);
}

int Pusher::getTotalParticleNumber(){
    return currentPartclNumOnDomain;
}

void Pusher::setTotalParticleNumber(int num){
    currentPartclNumOnDomain = num;
}


double Pusher::getParticleWeight4Type(int type){
    return weights[type];
}

double Pusher::getParticleCharge4Type(int type){
    return charges[type];
}


double Pusher::getParticleMass4Type(int type){
    return masses[type];
}




/*                        E(i,j+1)           |              E(i+1,j+1)
 *                           x-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-x
 *                           .               |               .
 *                           |               |   CELL i,j    |
 *                           .               |               .
 *                         dy/2              |   O(x,y)      |
 *                           .               |     ->        .
 *                           |               |i=int(x/dx+0.5)|
 *                           .               |j=int(y/dy+0.5).
 *                           |               |               |
 *            .B(i-1,j)______________________.B(i,j)____________________.B(i+1,j)
 *                           |               |               |
 *                           .               |               .
 *                           |               |               |
 *                         dy/2              |               .
 *                           |               |               |
 *                           .               |               .
 *                           |               |               |
 *                           . E(i,j)        |               .
 *                           x-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-x E(i+1,j)
 *                                  dx/2     |      dx/2
 */


Particle** Pusher::getParticles(){
    return particles;
}


void Pusher::push(int phase, int i_time){
    
   logger->writeMsg(("[Pusher] start pushing "+to_string(currentPartclNumOnDomain)
                                    +" particles...").c_str(), DEBUG);
    
    auto start_time = high_resolution_clock::now();
    
    double E[3], B[3];
    
    double dx = loader->spatialSteps[0];
    double dy = loader->spatialSteps[1];
    double dz = loader->spatialSteps[2];
    
    int xSize = loader->resolution[0];
    int ySize = loader->resolution[1];
    int zSize = loader->resolution[2];
    
    double domainShiftX = loader->boxCoordinates[0][0];
    double domainShiftY = loader->boxCoordinates[1][0];
    double domainShiftZ = loader->boxCoordinates[2][0];
    
    double F;
    double Fsquare;
    double Bsquare, G;
    
    double curV[3] = {0.0, 0.0, 0.0};
    double curU[3] = {0.0, 0.0, 0.0};
    double VBprod[3], UBprod[3];
    double new_position[3] = {0.0, 0.0, 0.0};
    double new_velocity[3] = {0.0, 0.0, 0.0};
    double ts = loader->getTimeStep();
    double charge, mass, qm;
    int coord, idx;
    
    double x, y, z;
    double lx4B, ly4B, lz4B, lx4E, ly4E, lz4E;
    int i4B, j4B, k4B, i4E, j4E, k4E;

    
    int idxG1, idxG2;
    int idx_x, idx_y, idx_z;
    double alpha, betta, gamma, weightE, weightB;
    double neighbourhood[8][3] = {{0,0,0}, {1,0,0}, {0,1,0}, {1,1,0},
                                  {0,0,1}, {1,0,1}, {0,1,1}, {1,1,1}};

    int type;
    
  
    VectorVar** Efield = gridMgr->getVectorVariableOnG2(ELECTRIC);
    VectorVar** Bfield = gridMgr->getVectorVariableOnG1(MAGNETIC);
    
    int G2nodesNumber = (xSize+2)*(ySize+2)*(zSize+2);
    int G1nodesNumber = (xSize+1)*(ySize+1)*(zSize+1);
    
    int posShift = 0;
    int velShift = 0;

    if(phase == CORRECTOR){
        posShift = 3;
        velShift = 3;
    }
    
    double* prtclPos;
    double* prtclVel;
    double cflvel[3];
    
    for(int coord=0; coord < 3; coord++){
        cflvel[coord] = loader->spatialSteps[coord]/ts;
    }
    //need to save previous value
    if(phase == PREDICTOR){
        for( int idx=0; idx < currentPartclNumOnDomain; idx++){
            prtclPos = particles[idx]->getPosition();
            for(int coord=0; coord < 3; coord++){
                // save corrector(+3) value
                particles[idx]->setPosition(coord+3, prtclPos[coord]);
            }
        }
    }
    
    for( int idx=0; idx < currentPartclNumOnDomain; idx++){
        
        prtclPos = particles[idx]->getPosition();
        prtclVel = particles[idx]->getVelocity();
        type     = particles[idx]->getType();
        
        for(int coord=0; coord < 3; coord++){
            E[coord] = 0.0;
            B[coord] = 0.0;
        }
        
        x  = prtclPos[0];// always use predictor value
        y  = prtclPos[1];
        z  = prtclPos[2];
        
        x = (x - domainShiftX)/dx;
        y = (y - domainShiftY)/dy;
        z = (z - domainShiftZ)/dz;
        
        // important for 1D and 2D cases
        x = ( x < xSize) ? x : xSize-EPS4;
        y = ( y < ySize) ? y : ySize-EPS4;
        z = ( z < zSize) ? z : zSize-EPS4;
        
        i4B  = int(x);
        j4B  = int(y);
        k4B  = int(z);
        
        lx4B = x-i4B;
        ly4B = y-j4B;
        lz4B = z-k4B;
        
        double alphasB[8] = {1.0-lx4B, lx4B, 1.0-lx4B, lx4B,
                             1.0-lx4B, lx4B, 1.0-lx4B, lx4B};
        
        double bettasB[8] = {1.0-ly4B, 1.0-ly4B, ly4B, ly4B,
                             1.0-ly4B, 1.0-ly4B, ly4B, ly4B};
        
        double gammasB[8] = {1.0-lz4B, 1.0-lz4B, 1.0-lz4B, 1.0-lz4B,
                                 lz4B,     lz4B,     lz4B,     lz4B};
        
        x  = prtclPos[0];// always use predictor value
        y  = prtclPos[1];
        z  = prtclPos[2];
        
        x = (x - domainShiftX)/dx+0.5;
        y = (y - domainShiftY)/dy+0.5;
        z = (z - domainShiftZ)/dz+0.5;
        
        i4E  = int(x);
        j4E  = int(y);
        k4E  = int(z);
        
        lx4E = x-i4E;
        ly4E = y-j4E;
        lz4E = z-k4E;
        
        double alphasE[8] = {1.0-lx4E, lx4E, 1.0-lx4E, lx4E,
                             1.0-lx4E, lx4E, 1.0-lx4E, lx4E};
        
        double bettasE[8] = {1.0-ly4E, 1.0-ly4E, ly4E, ly4E,
                             1.0-ly4E, 1.0-ly4E, ly4E, ly4E};
        
        double gammasE[8] = {1.0-lz4E, 1.0-lz4E, 1.0-lz4E, 1.0-lz4E,
                                 lz4E,     lz4E,     lz4E,     lz4E};

        
        for (int neigh_num=0; neigh_num < 8; neigh_num++){
        
            // - set b field on g1
            idx_x = i4B + neighbourhood[neigh_num][0];
            idx_y = j4B + neighbourhood[neigh_num][1];
            idx_z = k4B + neighbourhood[neigh_num][2];
            
            idxG1 = IDX(idx_x ,idx_y ,idx_z, xSize+1, ySize+1, zSize+1);
            
            alpha = lx4B;
            betta = ly4B;
            gamma = lz4B;
            //TODO problem for 2D and 1D add switch(loader->dim)
            alpha = alphasB[neigh_num];
            betta = bettasB[neigh_num];
            gamma = gammasB[neigh_num];
            weightB = alpha*betta*gamma;
            
            // - set e field on g2
            idx_x = i4E + neighbourhood[neigh_num][0];
            idx_y = j4E + neighbourhood[neigh_num][1];
            idx_z = k4E + neighbourhood[neigh_num][2];
            
            idxG2 = IDX(idx_x ,idx_y ,idx_z, xSize+2, ySize+2, zSize+2);
            
            alpha = alphasE[neigh_num];
            betta = bettasE[neigh_num];
            gamma = gammasE[neigh_num];
            weightE = alpha*betta*gamma;
 
            const double* ef = Efield[idxG2]->getValue();
            const double* bf = Bfield[idxG1]->getValue();
            for(int coord=0; coord < 3; coord++){
                E[coord] += weightE*ef[coord];
                B[coord] += weightB*bf[coord];
            }
        }
        
        charge = charges[type];
        mass = masses[type];
        qm = charge/mass;
        F = 0.5*qm*ts;
        Fsquare = F*F;
        Bsquare = B[0]*B[0]+B[1]*B[1]+B[2]*B[2];
        G = 2.0/(1.0+Bsquare*Fsquare);
        
        /* __ half acceleration in e field __ */
        curV[0] = prtclVel[0+velShift]+F*E[0];
        curV[1] = prtclVel[1+velShift]+F*E[1];
        curV[2] = prtclVel[2+velShift]+F*E[2];
        
        /* __ half rotation in b field __ */
        VBprod[0] = curV[1] * B[2] - curV[2] * B[1];
        VBprod[1] = curV[2] * B[0] - curV[0] * B[2];
        VBprod[2] = curV[0] * B[1] - curV[1] * B[0];
        
        curU[0] = curV[0] + F*VBprod[0];
        curU[1] = curV[1] + F*VBprod[1];
        curU[2] = curV[2] + F*VBprod[2];
        
        UBprod[0] = curU[1] * B[2] - curU[2] * B[1];
        UBprod[1] = curU[2] * B[0] - curU[0] * B[2];
        UBprod[2] = curU[0] * B[1] - curU[1] * B[0];
        
        for( coord=0; coord < 3; coord++){
            new_velocity[coord] = curV[coord]+(G*UBprod[coord]+E[coord])*F;
        }
        
//    # change velocities in all directions always
        for( coord=0; coord < 3; coord++){
            particles[idx]->setVelocity(velShift+coord, new_velocity[coord]);
        }
//    # change coordinates only in corresponding directions (1D - X, 2D - X/Y, 3D X/Y/Z)
        for( coord=0; coord < loader->dim; coord++){
            new_position[coord] = prtclPos[coord+posShift] + new_velocity[coord]*ts;
            particles[idx]->setPosition(posShift+coord, new_position[coord]);
        }

        int domainNum = boundaryMgr->isPtclOutOfDomain(new_position);
        if( domainNum != IN){
            prtclPos = particles[idx]->getPosition();
            double _pos[3] = {prtclPos[posShift+0],prtclPos[posShift+1],prtclPos[posShift+2]};
            boundaryMgr->storeParticle(idx, _pos);
        }
        
    }
    
    
    auto end_time = high_resolution_clock::now();
    string msgs ="[Pusher] solve(): before applying BC duration = "
                    +to_string(duration_cast<milliseconds>(end_time - start_time).count())+" ms";
    logger->writeMsg(msgs.c_str(),  DEBUG);
    
    vector<shared_ptr<Particle>> particles2add;
    particles2add.reserve(EXPECTED_NUM_OF_PARTICLES);
        
    boundaryMgr->applyBC(particles, particles2add, phase);
    vector<int> leavingParticles = boundaryMgr->getLeavingParticlesIdxs();
    int tot2add = particles2add.size();
    int tot2remove = leavingParticles.size();
    int prevNumOfPartcl = currentPartclNumOnDomain;
    
    
    for ( int i = 0; i < tot2add; i++ ) {
        if ( i < tot2remove ) {
            particles[leavingParticles[i]]->reinitializeUsingParticle(particles2add[i]);
        }else{
            particles[currentPartclNumOnDomain]->reinitializeUsingParticle(particles2add[i]);
            currentPartclNumOnDomain++;
        }
    }
    
    auto end_time11 = high_resolution_clock::now();
    string msg01 ="[Pusher] reinitialize existing duration = "
    +to_string(duration_cast<milliseconds>(end_time11 - end_time).count())+" ms";
    logger->writeMsg(msg01.c_str(),  DEBUG);
    
    //need to start from the end otherwise there is a risk
    //to damage particle inside with a particle that has to be removed from the end
    for(int i=tot2remove-1; i>=tot2add; i--){
            int idxLeave = leavingParticles[i];
            int idxToUse = currentPartclNumOnDomain-1;// last particle
            if(idxLeave == idxToUse){
                currentPartclNumOnDomain--;
            }else{
                particles[idxLeave]->reinitializeUsingParticle(particles[idxToUse]);
                currentPartclNumOnDomain--;
            }
     }
    
    
    boundaryMgr->reset();
    particles2add.clear();
    leavingParticles.clear();
    
    string msg003 ="[Pusher] tot2add = "+to_string(tot2add)
                        +"; tot2remove = "+to_string(tot2remove)+"; currentPartclNumOnDomain = "
                            +to_string(currentPartclNumOnDomain)+"; prevNumOfPartcl = "
                            +to_string(prevNumOfPartcl);
    logger->writeMsg(msg003.c_str(),  DEBUG);
    
    logger->writeMsg(("[Pusher] On Domain  "+to_string(currentPartclNumOnDomain)
                                    +" particles...").c_str(), DEBUG);
    
#ifdef LOG
    vector<int> brokenParticles;
    for( int idx=0; idx < currentPartclNumOnDomain; idx++){
        if( checkParticle(idx, particles[idx], "before end") == 1){
            brokenParticles.push_back(idx);
        }
    }
    int brokenParticlesNum = brokenParticles.size();
    for( int idx=0; idx < brokenParticlesNum; idx++){
            
        int prtclIdx = brokenParticles[brokenParticlesNum-1-idx];
        int idxToUse = currentPartclNumOnDomain-1;// last particle
        if (prtclIdx == idxToUse){
            currentPartclNumOnDomain--;
        }else{
            particles[prtclIdx]->reinitializeUsingParticle(particles[idxToUse]);
            currentPartclNumOnDomain--;
        }
    }
    int TOT_BROKEN_IN_BOX = 0;
    MPI_Allreduce(&brokenParticlesNum, &TOT_BROKEN_IN_BOX, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    logger->writeMsg(("[Pusher] broken particles in the box = "+to_string(TOT_BROKEN_IN_BOX)).c_str(),  DEBUG);
#endif
    
    int TOT_IN_BOX = 0;
    MPI_Allreduce(&currentPartclNumOnDomain, &TOT_IN_BOX, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    logger->writeMsg(("[Pusher] total particles in the box = "+to_string(TOT_IN_BOX)).c_str(),  DEBUG);

    
    end_time = high_resolution_clock::now();
    string msg ="[Pusher] pushing...OK: solve() duration = "
                +to_string(duration_cast<milliseconds>(end_time - start_time).count())+" ms";
    logger->writeMsg(msg.c_str(),  DEBUG);
        
}



//#################################### EXTRA LOG ##################################

int Pusher::checkParticle(int idx, Particle* prtcl, string suffix){
    
    double ts = loader->getTimeStep();
    
    double* prtclPos = prtcl->getPosition();
    double* prtclVel = prtcl->getVelocity();
    
    for(int comp=0; comp<3; comp++){
        
        double CFL_VEL = loader->spatialSteps[comp]/ts;
        
        int r0 = int((prtclPos[comp] - loader->boxCoordinates[comp][0])/loader->spatialSteps[comp]+0.5);
        int r1 = int((prtclPos[comp+3] - loader->boxCoordinates[comp][0])/loader->spatialSteps[comp]+0.5);
        
        if( (r0 < 0) || (r1 < 0) ||
           (r0 >= (loader->resolution[comp]+2))
           || (r1 >= (loader->resolution[comp]+2))
           || abs(prtclVel[comp]) > CFL_VEL
           || abs(prtclVel[comp+3]) > CFL_VEL){
            
            logger->writeMsg(("[Pusher] "+suffix+" prtclPos[0]  = "+to_string(prtclPos[0])
                              +"\n     prtclPos[1]  = "+to_string(prtclPos[1])
                              +"\n     prtclPos[2]  = "+to_string(prtclPos[2])
                              +"\n     prtclPos[3]  = "+to_string(prtclPos[3])
                              +"\n     prtclPos[4]  = "+to_string(prtclPos[4])
                              +"\n     prtclPos[5]  = "+to_string(prtclPos[5])
                              +"\n     prtclVel[0]  = "+to_string(prtclVel[0])
                              +"\n     prtclVel[1]  = "+to_string(prtclVel[1])
                              +"\n     prtclVel[2]  = "+to_string(prtclVel[2])
                              +"\n     prtclVel[3]  = "+to_string(prtclVel[3])
                              +"\n     prtclVel[4]  = "+to_string(prtclVel[4])
                              +"\n     prtclVel[5]  = "+to_string(prtclVel[5])
                              +"\n     r0  = "+to_string(r0)
                              +"\n     r1  = "+to_string(r1)
                              +"\n     cflvel[0]  = "+to_string(loader->spatialSteps[0]/ts)
                              +"\n     cflvel[1]  = "+to_string(loader->spatialSteps[1]/ts)
                              +"\n     cflvel[2]  = "+to_string(loader->spatialSteps[2]/ts)
                              +"\n     loader->boxCoordinates[0]  = "+to_string(loader->boxCoordinates[0][0])
                              +"\n     loader->boxCoordinates[1]  = "+to_string(loader->boxCoordinates[1][0])
                              +"\n     loader->boxCoordinates[2]  = "+to_string(loader->boxCoordinates[2][0])
                              +"\n     loader->boxSizes[0]  = "+to_string(loader->boxSizes[0])
                              +"\n     loader->boxSizes[1]  = "+to_string(loader->boxSizes[1])
                              +"\n     loader->boxSizes[2]  = "+to_string(loader->boxSizes[2])
                              +"\n     idx = "+to_string(idx)
                              ).c_str(), CRITICAL);
            return 1;
        }
    }
    return 0;

}

