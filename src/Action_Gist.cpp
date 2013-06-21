// Gist 
#include <cmath>
#include <iostream> // cout
#include <cstring> // memset
#include <fstream>
using namespace std;
#include "Action_Gist.h"
#include "CpptrajFile.h"
#include "CpptrajStdio.h"
#include "DataSet_integer.h"
#include "Box.h"
#include "StringRoutines.h" 
#include "Constants.h" // GASCONSTANT and SMALL

// CONSTRUCTOR
Action_Gist::Action_Gist() :
  CurrentParm_(0),
  watermodel_(false),
  useTIP3P_(false),
  useTIP4P_(false),
  useTIP4PEW_(false),
  doOrder_(false)
{
  mprintf("\tGIST: INIT \n");
  gridcntr_[0] = -1;
  gridcntr_[1] = -1;
  gridcntr_[2] = -1;
    
  gridorig_[0] = -1;
  gridorig_[1] = -1;
  gridorig_[2] = -1;
  
  gridspacn_ = 0;
 } 


void Action_Gist::Help() {
  mprintf("gist <watermodel>[{tip3p|tip4p|tip4pew}] [doorder] [gridcntr <xval> <yval> <zval>] [griddim <xval> <yval> <zval>] [gridspacn <spaceval>] [out <filename>] \n");
  mprintf("\tCalculate GIST between water molecules in selected site \n");
}


// Action_Gist::init()
Action::RetType Action_Gist::Init(ArgList& actionArgs, TopologyList* PFL, FrameList* FL,
				  DataSetList* DSL, DataFileList* DFL, int debugIn)
{
  mprintf("\tGIST: init \n");
  // Get keywords
  
  // Dataset to store gist results
  datafile_ = actionArgs.GetStringKey("out");
  // Generate the data set name, and hold onto the master data set list
  /*string ds_name = actionArgs.GetStringKey("name");
  ds_name = myDSL_.GenerateDefaultName("GIST");
  // We have 4?? data sets Add them here
  // Now add all of the data sets
  for (int i = 0; i < 4; i++) {
    myDSL_.AddSetAspect(DataSet::DOUBLE, ds_name,
			integerToString(i+1).c_str());
			}
  //  myDSL_.AddSet(DataSet::DOUBLE, ds_name, NULL);
  */  
  useTIP3P_ = actionArgs.hasKey("tip3p");
  useTIP4P_ = actionArgs.hasKey("tip4p");
  useTIP4PEW_ = actionArgs.hasKey("tip4pew");
  if (!useTIP3P_ && !useTIP4P_ && !useTIP4PEW_) {
    mprinterr("Error: gist: Only water models supprted are TIP3P and TIP4P\n");
    return Action::ERR;
  }

  doOrder_ = actionArgs.hasKey("doorder");
  if(doOrder_){
    mprintf("\tGIST doing Order calculation");
  }
  else{
    mprintf("\tGIST NOT doing Order calculation");
  }

  // Set Bulk Energy based on water model
  if (useTIP3P_) BULK_E_ = -19.0653;
  if (useTIP4P_ || useTIP4PEW_) BULK_E_ = -22.06;
  mprintf("\tGIST bulk energy: %10.5f\n", BULK_E_/2.0);
  
  // Set Bulk Density 55.5M
  BULK_DENS_ = 0.033422885325;
  mprintf("\tGIST bulk densiy: %15.12f\n", BULK_DENS_);

  if ( actionArgs.hasKey("gridcntr") ){
    gridcntr_[0] = actionArgs.getNextDouble(-1);
    gridcntr_[1] = actionArgs.getNextDouble(-1);
    gridcntr_[2] = actionArgs.getNextDouble(-1);
    mprintf("\tGIST grid center: %5.3f %5.3f %5.3f\n", gridcntr_[0],gridcntr_[1],gridcntr_[2]);
  }
  else{
    mprintf("\tGIST: No grid center values were found, using default\n");
    gridcntr_[0] = 0.0;
    gridcntr_[1] = 0.0;
    gridcntr_[2] = 0.0;
    mprintf("\tGIST grid center: %5.3f %5.3f %5.3f\n", gridcntr_[0],gridcntr_[1],gridcntr_[2]);
  }

  griddim_.clear();
  griddim_.resize( 3 );
  if ( actionArgs.hasKey("griddim") ){
    griddim_[0] = actionArgs.getNextInteger(-1);
    griddim_[1] = actionArgs.getNextInteger(-1);
    griddim_[2] = actionArgs.getNextInteger(-1);
    mprintf("\tGIST grid dimension: %d %d %d \n", griddim_[0],griddim_[1],griddim_[2]);
  }
  else{
    mprintf("\tGIST: No grid dimensiom values were found, using default (box size) \n");
    griddim_[0] = 40;
    griddim_[1] = 40;
    griddim_[2] = 40;
    mprintf("\tGIST grid dimension: %d %d %d \n", griddim_[0],griddim_[1],griddim_[2]);
  }

  gridspacn_ = actionArgs.getKeyDouble("gridspacn", 0.50);
  mprintf("\tGIST grid spacing: %5.3f \n", gridspacn_);

  bool imageIn=1;
  InitImaging(imageIn);
  
  return Action::OK;
}

// Action_Gist::setup()
/** Set Gist up for this parmtop. Get masks etc.
  */
Action::RetType Action_Gist::Setup(Topology* currentParm, Topology** parmAddress) {
  mprintf("GIST Setup \n");
  
  CurrentParm_ = currentParm;      
  NFRAME_ = 0;
  max_nwat_ = 0;

  MAX_GRID_PT_ = griddim_[0] * griddim_[1] * griddim_[2];
  Vvox_ = gridspacn_*gridspacn_*gridspacn_;
  /*G_max_x = griddim_[0] * gridspacn_ + 1.5 ;
  G_max_y = griddim_[1] * gridspacn_ + 1.5 ;
  G_max_z = griddim_[2] * gridspacn_ + 1.5 ;
  */
  mprintf("\tGIST Setup: %d %d %d %d %f \n", griddim_[0], griddim_[1], griddim_[2], MAX_GRID_PT_, Vvox_);

  // Set up grid origi
  gridorig_[0] = gridcntr_[0] - 0.5*griddim_[0]*gridspacn_;
  gridorig_[1] = gridcntr_[1] - 0.5*griddim_[1]*gridspacn_;
  gridorig_[2] = gridcntr_[2] - 0.5*griddim_[2]*gridspacn_;
  mprintf("\tGIST grid origin: %5.3f %5.3f %5.3f\n", gridorig_[0], gridorig_[1], gridorig_[2]);

  // Set up cumulative energy arrays
  x_.clear();
  x_.resize(5, 0.0);
  y_.clear();
  y_.resize(5, 0.0);
  z_.clear();
  z_.resize(5, 0.0);
  wh_evdw_.clear();
  wh_evdw_.resize(MAX_GRID_PT_, 0.0);
  wh_eelec_.clear();
  wh_eelec_.resize(MAX_GRID_PT_, 0.0);
  ww_evdw_.clear();
  ww_evdw_.resize(MAX_GRID_PT_, 0.0);
  ww_eelec_.clear();
  ww_eelec_.resize(MAX_GRID_PT_, 0.0);

  //voxel coords
  grid_x_.clear();    
  grid_x_.resize(MAX_GRID_PT_, 0.0); 
  grid_y_.clear();		      
  grid_y_.resize(MAX_GRID_PT_, 0.0);
  grid_z_.clear();		      
  grid_z_.resize(MAX_GRID_PT_, 0.0); 


  // get the actual voxel coordinates
  voxel=0;
  for (int i = 0; i < griddim_[0]; ++i) {
    for (int j = 0; j < griddim_[1]; ++j) {
      for (int k = 0; k < griddim_[2]; ++k) {
        grid_x_[voxel] = Xcrd(i);
        grid_y_[voxel] = Ycrd(j);
        grid_z_[voxel] = Zcrd(k);
        voxel++;
      }
    }
  }

  dEwh_dw_.clear();
  dEwh_dw_.resize(MAX_GRID_PT_, 0.0);
  dEww_dw_ref_.clear();
  dEww_dw_ref_.resize(MAX_GRID_PT_, 0.0);
  dEwh_norm_.clear();
  dEwh_norm_.resize(MAX_GRID_PT_, 0.0);
  dEww_norm_ref_.clear();
  dEww_norm_ref_.resize(MAX_GRID_PT_, 0.0);

  ww_Eij_.clear();
  ww_Eij_.resize(MAX_GRID_PT_);
  //for(int i = 0; i < MAX_GRID_PT_; i++) ww_Eij_[i].resize(MAX_GRID_PT_);
  for(int i = 1; i < MAX_GRID_PT_; i++) ww_Eij_[i].resize(i);

  //CN: need to initialize ww_Eij_ to 0.0 but not Euler angles
  //for (int a=0; a<MAX_GRID_PT_; a++)
    //for (int l=0; l<MAX_GRID_PT_; l++) ww_Eij_[a][l]=0.0;
  for (int a=1; a<MAX_GRID_PT_; a++)
    for (int l=0; l<a; l++) ww_Eij_[a][l]=0.0;  

  the_vox_.clear();
  the_vox_.resize(MAX_GRID_PT_);
  phi_vox_.clear();
  phi_vox_.resize(MAX_GRID_PT_);
  psi_vox_.clear();
  psi_vox_.resize(MAX_GRID_PT_);

  TStrans_dw_.clear();
  TStrans_dw_.resize(MAX_GRID_PT_, 0.0);
  TStrans_norm_.clear();
  TStrans_norm_.resize(MAX_GRID_PT_, 0.0); 
  TSNN_dw_.clear();
  TSNN_dw_.resize(MAX_GRID_PT_, 0.0);
  TSNN_norm_.clear();
  TSNN_norm_.resize(MAX_GRID_PT_, 0.0);

  nwat_.clear();
  nwat_.resize(MAX_GRID_PT_, 0);
  nH_.clear();
  nH_.resize(MAX_GRID_PT_, 0);
  nw_angle_.clear();
  nw_angle_.resize(MAX_GRID_PT_, 0);
  dens_.clear();
  dens_.resize(MAX_GRID_PT_, 0.0);
  g_.clear();
  g_.resize(MAX_GRID_PT_, 0.0);
  gH_.clear();
  gH_.resize(MAX_GRID_PT_, 0.0);
  dipolex_.clear();
  dipolex_.resize(MAX_GRID_PT_, 0.0);
  dipoley_.clear();
  dipoley_.resize(MAX_GRID_PT_, 0.0);
  dipolez_.clear();
  dipolez_.resize(MAX_GRID_PT_, 0.0);
  neighbor_.clear();
  neighbor_.resize(MAX_GRID_PT_, 0);
  qtet_.clear();
  qtet_.resize(MAX_GRID_PT_, 0.0);

  gridwat_.clear();
  gridwat_.resize( currentParm->Natom() );

  // We need box info
  if (currentParm->BoxType() == Box::NOBOX) {
    mprinterr("Error: Gist: Must have explicit solvent with periodic boundaries!");
    return Action::ERR;
  }
  SetupImaging( currentParm->BoxType() );

  resnum =0;
  voxel =0;

  return Action::OK;  
}


// Action_Gist::action()
Action::RetType Action_Gist::DoAction(int frameNum, Frame* currentFrame, Frame** frameAddress) {

  NFRAME_ ++;
  if (NFRAME_==1) mprintf("GIST Action \n");

  // Simulation box length - assign here because it can vary for npt simulation
  Lx = currentFrame->BoxCrd().BoxX();
  Ly = currentFrame->BoxCrd().BoxY();
  Lz = currentFrame->BoxCrd().BoxZ();
  if (NFRAME_==1) mprintf("GIST Action box length: %f %f %f \n", Lx, Ly, Lz);
  
  int solventMolecules = CurrentParm_->Nsolvent();
  resnum =0;
  voxel =0;
  for (solvmol = CurrentParm_->MolStart();
       solvmol != CurrentParm_->MolEnd(); ++solvmol)
    {
      if (!(*solvmol).IsSolvent()) continue;
      Grid( currentFrame );
      voxel = gridwat_[resnum];
      resnum++;   
      if (voxel>=MAX_GRID_PT_) continue;
      EulerAngle( currentFrame );
      Dipole( currentFrame );
      NonbondEnergy( currentFrame );
      //if(doOrder_) Order( currentFrame );

    }
  
  //Debugg
  if (NFRAME_==1) mprintf("GIST  DoAction:  Found %d solvent residues \n", resnum);
  if (solventMolecules != resnum) {
    mprinterr("GIST  DoAction  Error: No solvent molecules don't match %d %d\n", solventMolecules, resnum);
  }
  
  return Action::OK;
}


static void GetLJparam(Topology const& top, double& A, double& B, 
                              int atom1, int atom2)
{
  // In Cpptraj, atom numbers start from 1, so subtract 1 from the NB index array
  int param = (top.Ntypes() * (top[atom1].TypeIndex()-1)) + top[atom2].TypeIndex()-1;
  int index = top.NB_index()[param] - 1;
  A = top.LJA()[index];
  B = top.LJB()[index];
}


void Action_Gist::NonbondEnergy(Frame *currentFrame) {
  double delta2, Acoef, Bcoef, deltatest;
  Vec3 XYZ, XYZ2, JI;
  double rij2, rij, r2, r6, r12, f12, f6, e_vdw, qiqj, e_elec;
  int satom, satom2;//,param,index;

  //  Topology::mol_iterator solvmol, solvmol2;
  //if (NFRAME_==1) mprintf("GIST NonbondEnergy \n");
  int  voxel2,  resnum2;
  double q1, q2;

  //variables for Order  
  int  i,atom1,atom2,mol1,mol2;
  double cos, sum, r_1, r_2, r_3, r_4;//, x_[5], y_[5], z_[5];
  Vec3 O_wat1, O_wat2, O_wat3, v1, v2;
  r_1=1000; r_2=1000; r_3=1000; r_4=1000;
  x_[0] = 0.0; x_[1] = 0.0; x_[2] = 0.0; x_[3] = 0.0; x_[4] = 0.0;
  y_[0] = 0.0; y_[1] = 0.0; y_[2] = 0.0; y_[3] = 0.0; y_[4] = 0.0;
  z_[0] = 0.0; z_[1] = 0.0; z_[2] = 0.0; z_[3] = 0.0; z_[4] = 0.0;
  //if (NFRAME_==1 && doOrder_) mprintf("GIST Order Parameter \n");
  
  // Setup imaging info
  //Matrix_3x3 ucell, recip;
  // Loop over solvent atoms
  atom1 = 0;
  for (satom = (*solvmol).BeginAtom(); satom < (*solvmol).EndAtom(); ++satom)
    {
      // Set up coord index for this atom
      XYZ =  Vec3(currentFrame->XYZ( satom ));	  
      // Inner loop	has both solute and solvent
      resnum2=0;
      //initialization for order
      
      for (solvmol2 = CurrentParm_->MolStart();
	   solvmol2 != CurrentParm_->MolEnd(); ++solvmol2)
	{
	  if ((*solvmol2).IsSolvent()) { 
	    // Solvent loop
	    // skip water intearction with with itself
	    voxel2 = gridwat_[resnum2];
	    resnum2++;
	    if ((resnum-1) == (resnum2-1)) continue;
	  }
	  
	  
	  atom2 = 0;
	  for (satom2 = (*solvmol2).BeginAtom(); satom2 < (*solvmol2).EndAtom(); ++satom2)
	    {    
	      // Set up coord index for this atom
	      XYZ2 = Vec3(currentFrame->XYZ( satom2 ));
	      // Calculate the vector pointing from atom2 to atom1
	      //rij2 = DIST2_ImageOrtho(XYZ, XYZ2, currentFrame->BoxCrd());
	      switch( ImageType() ) {
	      case NONORTHO:
		currentFrame->BoxCrd().ToRecip(ucell, recip);
		rij2 = DIST2_ImageNonOrtho(XYZ, XYZ2, ucell, recip);
		break;
	      case ORTHO:
		rij2 = DIST2_ImageOrtho(XYZ, XYZ2, currentFrame->BoxCrd());
		break;
	      default:
		rij2 = DIST2_NoImage(XYZ, XYZ2);	          
	      }
	      rij = sqrt(rij2);
	      // LJ energy 
	      GetLJparam(*CurrentParm_, Acoef, Bcoef, satom, satom2);
	      
	      r2    = 1 / rij2;
	      r6    = r2 * r2 * r2;
	      r12   = r6 * r6;
	      f12   = Acoef * r12;  // A/r^12
	      f6    = Bcoef * r6;   // B/r^6
	      e_vdw = f12 - f6;     // (A/r^12)-(B/r^6)
	      // LJ Force 
	      //force=((12*f12)-(6*f6))*r2; // (12A/r^13)-(6B/r^7)
	      // Coulomb energy 
	      q1 = (*CurrentParm_)[satom].Charge() * ELECTOAMBER;
	      q2 = (*CurrentParm_)[satom2].Charge() * ELECTOAMBER;
	      e_elec = (q1*q2/rij);
	      //e_elec = (qiqj/rij);
	      if (!(*solvmol2).IsSolvent()) {
		// solute-solvent interaction
		wh_evdw_[voxel] +=  e_vdw;
		wh_eelec_[voxel] += e_elec;
	      }
	      else {
		// solvent-solvent interaction
		ww_evdw_[voxel] +=  e_vdw;
		ww_eelec_[voxel] += e_elec;
		// CN: only store Eij[voxel1][voxel2] if both voxels lie on the grid.
		if (voxel2<MAX_GRID_PT_) {
		  if (voxel>voxel2) {
		    ww_Eij_[voxel][voxel2] += e_vdw*0.5;
		    ww_Eij_[voxel][voxel2] += e_elec*0.5;
		  }
		  else {
		    ww_Eij_[voxel2][voxel] += e_vdw*0.5;
		    ww_Eij_[voxel2][voxel] += e_elec*0.5;
		  }  
		}
		
		// Store the water neighbor only used O-O distance
		//if (satom2==0 && satom==0 && rij<3.5) {
		if (atom1==0 && atom2==0 && rij<3.5) {
		  neighbor_[voxel]++;
		  if (voxel2<MAX_GRID_PT_) neighbor_[voxel2]++;	
		}
		
		// Action_Gist::Order() 
		if((doOrder_) && (atom1 == 0) && (atom2 == 0)) {
		  		  
		  O_wat2 = XYZ2;      
		  O_wat1 = XYZ;      

		  if (rij2<r_1) {
		    r_4 = r_3; x_[4] = x_[3]; y_[4] = y_[3]; z_[4] = z_[3];
		    r_3 = r_2; x_[3] = x_[2]; y_[3] = y_[2]; z_[3] = z_[2]; 
		    r_2 = r_1; x_[2] = x_[1]; y_[2] = y_[1]; z_[2] = z_[1];
		    r_1 = rij2; x_[1] = O_wat2[0]; y_[1] = O_wat2[1]; z_[1] = O_wat2[2];
		  }
		  
		}// END if doOrder_
	      }// IF solvent
	      atom2++;
	    } // END Inner loop ALL atoms 
	} // END Inner loop ALL molecules
      if((doOrder_) && (atom1 == 0) && (atom2 == 0)) {
	
	// Compute the tetahedral order parameter
	sum=0;
	for (mol1=1; mol1<=3; mol1++) {
	  for (mol2=1; mol2<=4; mol2++) {
	    if (mol1==mol2) continue;
	    O_wat2[0] = x_[mol1];
	    O_wat2[1] = y_[mol1];
	    O_wat2[2] = z_[mol1];
	    O_wat3[0] = x_[mol2];
	    O_wat3[1] = y_[mol2];
	    O_wat3[2] = z_[mol2];
	    v1 = O_wat2 - O_wat1;
	    v2 = O_wat3 - O_wat1; 	 
	    cos = v1*( v2);
	    sum += (cos + 1.0/3)*(cos + 1.0/3);
	  }
	}
	qtet_[voxel] += 1 - (3.0/8)*sum;
      }// END if doOrder_
      atom1++;
    } // END Outer loop solvent atoms
}


// Action_Gist::Grid()
void Action_Gist::Grid(Frame *frameIn) {

  int  i, gridindex[3];
  Vec3 comp, O_wat, atom, H1_wat, H2_wat;
  double rij;
  i = (*solvmol).BeginAtom();
  O_wat = Vec3(frameIn->XYZ(i));
  H1_wat = Vec3(frameIn->XYZ(i+1));
  H2_wat = Vec3(frameIn->XYZ(i+2));
  
  //      gridwat_[resnum] = 100000000;
  gridwat_[resnum] = MAX_GRID_PT_ + 1;
  for (int a=0; a<=2; a++) {
    if (a==0) atom = O_wat;
    else if (a==1) atom = H1_wat;
    else atom = H2_wat;
    // get the components of the water vector
    comp = Vec3(atom) - Vec3(gridorig_);
    comp /= gridspacn_;

    //Crystal, comment this if and the next three lines if you want to return to old code
    if (comp[0]>=0 && comp[1]>=0 && comp[2]>=0 ){
      gridindex[0] = (int) comp[0];
      gridindex[1] = (int) comp[1];
      gridindex[2] = (int) comp[2];
      
      if ((gridindex[0]<griddim_[0]) && (gridindex[1]<griddim_[1]) && (gridindex[2]<griddim_[2]))
	{
	  // this water belongs to grid point gridindex[0], gridindex[1], gridindex[2]
	  voxel = (gridindex[0]*griddim_[1] + gridindex[1])*griddim_[2] + gridindex[2];
	  if (a==0) {
	    gridwat_[resnum] = voxel;
	    nwat_[voxel]++;
	    if (max_nwat_ < nwat_[voxel]) max_nwat_ = nwat_[voxel];
	  }
	  else nH_[voxel]++;
	  //mprintf("fm=%d, resnum=%d, voxel=%d %d\n", NFRAME_, resnum, voxel, gridwat_[resnum]);
	}
      }
  } 
}

void Action_Gist::EulerAngle(Frame *frameIn) {

  //if (NFRAME_==1) mprintf("GIST Euler Angles \n");
  Vec3 x_lab, y_lab, z_lab, O_wat, H1_wat, H2_wat, x_wat, y_wat, z_wat, node, v;
  double cp, dp;

  int i = (*solvmol).BeginAtom();
  O_wat = Vec3(frameIn->XYZ(i));
  H1_wat = Vec3(frameIn->XYZ(i+1)) - O_wat;
  H2_wat = Vec3(frameIn->XYZ(i+2)) - O_wat;
  
  // make sure the first three atoms are oxygen followed by two hydrogen
  if ((*CurrentParm_)[i].Element() != Atom::OXYGEN) {
    cout << "Bad! coordinates do not belong to oxygen atom " << (*CurrentParm_)[i].ElementName() << endl;
  }
  if ((*CurrentParm_)[i+1].Element() != Atom::HYDROGEN || (*CurrentParm_)[i+2].Element() != Atom::HYDROGEN) {
    cout << "Bad! coordinates do not belong to oxygen atom " << (*CurrentParm_)[i+1].ElementName() << " " << (*CurrentParm_)[i+2].ElementName() << endl;
  } 
  
  // Define lab frame of reference
  x_lab[0]=1.0; x_lab[1]=0; x_lab[2]=0;
  y_lab[0]=0; y_lab[1]=1.0; y_lab[2]=0;
  z_lab[0]=0; z_lab[1]=0; z_lab[2]=1.0;     
  
  // Define the water frame of reference - all axes must be normalized
  // make h1 the water x-axis (but first need to normalized)
  x_wat = H1_wat;
  cp = x_wat.Normalize();
  // the normalized z-axis is the cross product of h1 and h2 
  z_wat = x_wat.Cross( H2_wat );
  cp = z_wat.Normalize();
  // make y-axis as the cross product of h1 and z-axis
  y_wat = z_wat.Cross( x_wat );
  cp = y_wat.Normalize();
  
  // Find the X-convention Z-X'-Z'' Euler angles between the water frame and the lab/host frame
  // First, theta = angle between the water z-axis of the two frames
  dp = z_lab*( z_wat);
  theta = acos(dp);
  if (theta>0 && theta<PI) {
    // phi = angle between the projection of the water x-axis and the node
    // line of node is where the two xy planes meet = must be perpendicular to both z axes
    // direction of the lines of node = cross product of two normals (z axes)
    // acos of x always gives the angle between 0 and pi, which is okay for theta since theta ranges from 0 to pi
    node = z_lab.Cross( z_wat );
    cp = node.Normalize();
    
    // Second, find the angle phi, which is between x_lab and the node
    dp = node*( x_lab );
    if (dp <= -1.0) phi = PI;
    else if (dp >= 1.0) phi = PI;
    else phi = acos(dp);
    // check angle phi
    if (phi>0 && phi<(2*PI)) {
      // method 2
      v = x_lab.Cross( node );
      dp = v*( z_lab );
      if (dp<0) phi = 2*PI - phi;
    }
    
    // Third, rotate the node to x_wat about the z_wat axis by an angle psi
    // psi = angle between x_wat and the node 
    dp = x_wat*( node );
    if (dp<=-1.0) psi = PI;
    else if (dp>=1.0) psi = 0;
    else psi = acos(dp);
    // check angle psi
    if (psi>0 && psi<(2*PI)) {
      // method 2
      Vec3 v = node.Cross( x_wat );
      dp = v*( z_wat );
      if (dp<0) psi = 2*PI - psi;
    }
    
    if (!(theta<=PI && theta>=0 && phi<=2*PI && phi>=0 && psi<=2*PI && psi>=0)) {
      cout << "angles: " << theta << " " << phi << " " << psi << endl;
      cout << H1_wat[0] << " " << H1_wat[1] << " " << H1_wat[2] << " " << H2_wat[0] << " " << H2_wat[1] << " " << H2_wat[2] << endl;
      mprinterr("Error: Euler: angles don't fall into range.\n");
      //break; 
    }
    
    the_vox_[voxel].push_back(theta);
    phi_vox_[voxel].push_back(phi);
    psi_vox_[voxel].push_back(psi);
    nw_angle_[voxel]++;
  }
  else cout << resnum-1 << " gimbal lock problem, two z_wat paralell" << endl;
} 


// Action_Gist::Dipole()
void Action_Gist::Dipole(Frame *frameIn) {
  
  //if (NFRAME_==1) mprintf("GIST Dipole \n");
  double dipolar_vector[3];
  Vec3 XYZ, sol;

  dipolar_vector[0] = 0.0;
  dipolar_vector[1] = 0.0;
  dipolar_vector[2] = 0.0;
  // Loop over solvent atoms
  for (int satom = (*solvmol).BeginAtom(); satom < (*solvmol).EndAtom(); ++satom)
    {
      XYZ = Vec3(frameIn->XYZ( satom ));
      sol[0] = XYZ[0] - gridorig_[0];
      sol[1] = XYZ[1] - gridorig_[1];
      sol[2] = XYZ[2] - gridorig_[2];
      //cout << NFRAME_ << " " << solvmol << " " << satom << sol[0] << " " << sol[1] << " " << sol[2] << endl;
      // Calculate dipole vector. The oxygen of the solvent is used to assign the voxel index to the water.
      // NOTE: the total charge on the solvent should be neutral for this to have any meaning
      
      double charge = (*CurrentParm_)[satom].Charge();
      dipolar_vector[0] += (charge * sol[0]);
      dipolar_vector[1] += (charge * sol[1]);
      dipolar_vector[2] += (charge * sol[2]);
    }
  voxel = gridwat_[resnum-1];
  dipolex_[voxel] += dipolar_vector[0];
  dipoley_[voxel] += dipolar_vector[1];
  dipolez_[voxel] += dipolar_vector[2];
}


void Action_Gist::Print() {
  
  // Implement NN to compute orientational entropy for each voxel
  double NNr, rx, ry, rz, rR, dbl;
  TSNNtot_=0;
  for (int gr_pt=0; gr_pt<MAX_GRID_PT_; gr_pt++) {
    TSNN_dw_[gr_pt]=0; TSNN_norm_[gr_pt]=0;  
    int nwtot = nw_angle_[gr_pt];
    if (nwtot<=1) continue;
    for (int n=0; n<nwtot; n++) {
      NNr=10000;
      for (int l=0; l<nwtot; l++) {
        if (l==n) continue;
        rx = cos(the_vox_[gr_pt][l]) - cos(the_vox_[gr_pt][n]);
        ry = phi_vox_[gr_pt][l] - phi_vox_[gr_pt][n];
        rz = psi_vox_[gr_pt][l] - psi_vox_[gr_pt][n];
        if (ry>PI) ry = 2*PI-ry;
        else if (ry<-PI) ry = 2*PI+ry;
        if (rz>PI) rz = 2*PI-rz;
        else if (rz<-PI) rz = 2*PI+rz;
        rR = sqrt(rx*rx + ry*ry + rz*rz);
        if (rR>0 && rR<NNr) NNr = rR;
      }
      if (NNr<9999 && NNr>0) {
	dbl = log(NNr*NNr*NNr*nwtot/(3.0*2*PI));
	TSNN_norm_[gr_pt] += dbl;
      }	
    }
    TSNN_norm_[gr_pt] = GASCONSTANT*0.3*0.239*(TSNN_norm_[gr_pt]/nwtot+0.5772);
    TSNN_dw_[gr_pt] = TSNN_norm_[gr_pt]*nwat_[gr_pt]/(NFRAME_*Vvox_);
    TSNNtot_ += TSNN_dw_[gr_pt];
  }
  TSNNtot_ *= Vvox_;
  mprintf("Max number of water = %d \n", max_nwat_);
  mprintf("Total orientational entropy of the grid: S_NN = %9.5f kcal/mol, Nf=%d\n", TSNNtot_, NFRAME_);
  
  // Compute translational entropy for each voxel
  TStranstot_=0;
  for (int a=0; a<MAX_GRID_PT_; a++) {
    dens_[a] = 1.0*nwat_[a]/(NFRAME_*Vvox_);
    g_[a] = dens_[a]/BULK_DENS_;
    gH_[a] = 1.0*nH_[a]/(NFRAME_*Vvox_*2*BULK_DENS_);
    if (nwat_[a]>1) {
       TStrans_dw_[a] = -GASCONSTANT*BULK_DENS_*0.3*0.239*g_[a]*log(g_[a]);
       TStrans_norm_[a] = TStrans_dw_[a]/dens_[a];
       TStranstot_ += TStrans_dw_[a];
    }
    else {
       TStrans_dw_[a]=0; TStrans_norm_[a]=0;
    }
  }
  TStranstot_ *= Vvox_;
  mprintf("Total translational entropy of the grid: S_trans = %9.5f kcal/mol, Nf=%d\n", TStranstot_, NFRAME_);

  // Compute average voxel energy
  for (int a=0; a<MAX_GRID_PT_; a++) {
    if (nwat_[a]>1) {
       dEwh_dw_[a] = (wh_evdw_[a]+wh_eelec_[a])/(NFRAME_*Vvox_);
       dEww_dw_ref_[a] = ((ww_evdw_[a]+ww_eelec_[a]) - nwat_[a]*BULK_E_)/(NFRAME_*Vvox_);
       dEwh_norm_[a] = (wh_evdw_[a]+wh_eelec_[a])/nwat_[a];
       dEww_norm_ref_[a] = (ww_evdw_[a]+ww_eelec_[a])/nwat_[a] - BULK_E_;  
    }
    else {
       dEwh_dw_[a]=0; dEww_dw_ref_[a]=0; dEwh_norm_[a]=0; dEww_norm_ref_[a]=0;
    }
  }

  // Print the gist info file
  // Print the energy data
  if (!datafile_.empty()) {
    // Now write the data file with all of the GIST energies
    DataFile dfl;
    ArgList dummy;
    dfl.SetupDatafile(datafile_, dummy, 0);
    for (int i = 0; i < myDSL_.size(); i++) {
      dfl.AddSet(myDSL_[i]);
    }

    dfl.Write();

  }
  PrintDX("gist-g.dx", g_);
  PrintDX("gist-gH.dx", gH_);
  PrintDX("gist-dEwh.dx", dEwh_dw_);
  PrintDX("gist-dEww.dx", dEww_dw_ref_);
  PrintDX("gist-TStrans.dx", TStrans_dw_);
  PrintDX("gist-TSorient.dx", TSNN_dw_); 
  PrintOutput("gist-output.dat");
  
}

  // Print GIST data in dx format
void Action_Gist::PrintDX(string const& filename, std::vector<double>& data)
{
  CpptrajFile outfile;
  if (outfile.OpenWrite(filename)) {
    mprinterr("Error: Could not open OpenDX output file.\n");
    return;
  }
  // Print the OpenDX header
  outfile.Printf("object 1 class gridpositions counts %d %d %d\n",
                 griddim_[0], griddim_[1], griddim_[2]);
  outfile.Printf("origin %lg %lg %lg\n", gridorig_[0], gridorig_[1], gridorig_[2]);
  outfile.Printf("delta %lg 0 0\n", gridspacn_);
  outfile.Printf("delta 0 %lg 0\n", gridspacn_);
  outfile.Printf("delta 0 0 %lg\n", gridspacn_);
  outfile.Printf("object 2 class gridconnections counts %d %d %d\n",
                 griddim_[0], griddim_[1], griddim_[2]);
  outfile.Printf(
    "object 3 class array type double rank 0 items %d data follows\n",
    MAX_GRID_PT_);

  // Now print out the data. It is already in row-major form (z-axis changes
  // fastest), so no need to do any kind of data adjustment
  for (int i = 0; i < MAX_GRID_PT_ - 2; i += 3)
    outfile.Printf("%g %g %g\n", data[i], data[i+1], data[i+2]);
  // Print out any points we may have missed
  switch (MAX_GRID_PT_ % 3) {
    case 2: outfile.Printf("%g %g\n", data[MAX_GRID_PT_-2], data[MAX_GRID_PT_-1]); break;
    case 1: outfile.Printf("%g\n", data[MAX_GRID_PT_-1]); break;
  }

  outfile.CloseFile();
}

  // Print GIST data in dx format
void Action_Gist::PrintOutput(string const& filename)
{
  /*CpptrajFile outfile;
  if (outfile.OpenWrite(filename)) {
    mprinterr("Error: Could not open Gist output file.\n");
    return;
  }
  // Print the Output header
  outfile.Printf("GIST Output, information printed per voxel\n");
  outfile.Printf("xcoord ycoord zcoord population density normalized-density S-trans S-trans-normalized S-rot S-rot-normalized E-water-solute E-water-solute-normalized E-water-water E-water-water-normalized \n");
  
  // Now print out the data. 
  for (int i = 0; i < MAX_GRID_PT_ ; i++){
    outfile.Printf(" %10.5f %10.5f %10.5f\n %10.5f %10.5f %10.5f\n %10.5f %10.5f %10.5f %10.5f %10.5f %10.5f %10.5f %10.5f\n",
		   grid_x_[i] , grid_y_[i], grid_z_[i], nwat_[i], dens_[i],g_[i], TStrans_dw_[i], TStrans_norm_[i], TSNN_dw_[i], TSNN_norm_[i], dEwh_dw_[i],dEwh_norm_[i],dEww_dw_ref_[i],dEww_norm_ref_[i]);
  }
  
  outfile.CloseFile();
  */
  ofstream myfile;
  myfile.open("gist-output.dat");  
  myfile << "GIST Output, information printed per voxel\n";
  myfile << "xcoord ycoord zcoord population normalized-density normalized-H_density";
  myfile << "TStrans TStrans-normalized TSrot TSrot-normalized E-water-solute";
  myfile << "E-water-solute-normalized E-water-water E-water-water-normalized Dipole_x Dipole_y Dipole_z neighbor\n";
  // Now print out the data. 
  for (int i = 0; i < MAX_GRID_PT_ ; i++){
    myfile << grid_x_[i] << " " << grid_y_[i]<< " " << grid_z_[i]<< " " << nwat_[i] << " " << g_[i]<< " " << gH_[i] << " ";
    myfile <<  TStrans_dw_[i]<< " " << TStrans_norm_[i]<< " " << TSNN_dw_[i] << " " << TSNN_norm_[i]<< " " << dEwh_dw_[i]<< " ";
    myfile << dEwh_norm_[i]<< " " << dEww_dw_ref_[i]<< " " << dEww_norm_ref_[i] << " " << dipolex_[i] << " " << dipoley_[i] << " " << dipolez_[i];
    myfile << 1.0*neighbor_[i]/NFRAME_ << " " << endl;
  }
  myfile.close();

  ofstream outfile;
  outfile.open("ww_Eij.dat");
  double dbl;
  for (int a=1; a<MAX_GRID_PT_; a++) {
    for (int l=0; l<a; l++) {
      dbl = ww_Eij_[a][l];
      if (dbl!=0) {
        dbl /= NFRAME_;
        //outfile.printf("%10d %10d %10.5f\n", a, l, dbl);
        //outfile << left << setw(12) << a << l << setprecision(7) << dbl << endl;
        outfile << a << " " << l << " " << dbl << endl;
      }
    }
  }
  outfile.close();
}

// DESTRUCTOR
Action_Gist::~Action_Gist() {
  //fprintf(stderr,"Gist Destructor.\n");
    gridwat_.clear();		// voxel index of each water
    nwat_.clear();		// total number of water found in each voxel
    nH_.clear();			// total number of hydrogen found in each voxel
    nw_angle_.clear();	// total nuber of Euler angles found in each voxel
    g_.clear();		// normalized water density
    gH_.clear();		// normalized H density
    dens_.clear();		// water density
    grid_x_.clear();	// voxel index in x
    grid_y_.clear();
    grid_z_.clear();
    neighbor_.clear();		// number of water neighbor within 3.5A
    qtet_.clear();		// tetahedral order parameter
    
    wh_evdw_.clear();
    wh_eelec_.clear();
    ww_evdw_.clear();
    ww_eelec_.clear();
    ww_Eij_.clear();
    dEwh_dw_.clear();
    dEww_dw_ref_.clear();
    dEwh_norm_.clear();
    dEww_norm_ref_.clear();
    
    TSNN_dw_.clear();
    TSNN_norm_.clear();
    TStrans_dw_.clear();
    TStrans_norm_.clear();

    the_vox_.clear();
    phi_vox_.clear();
    psi_vox_.clear();

    // dipole stuffs
    dipolex_.clear();
    dipoley_.clear();
    dipolez_.clear();
}
