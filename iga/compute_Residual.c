#undef __FUNCT__
#define __FUNCT__ "compute_Residual"

PetscErrorCode compute_Residual(SNES snes, Vec Ui_, Vec Residual_, void *app_)
{
    int ierr;

    /* ================ input parameters ================ */
    App *app=(App*)app_;
    // ---- PDE
    int ndim             = app->ndim;
    int nddim            = app->nddim;
    int ndof             = app->ndof;
    double *par_mat      = app->par_mat;
    int *bc_type         = app->bc_type;
    double *par_dirichlet= app->par_dirichlet;
    double *par_neumann  = app->par_neumann;
    int nboun            = app->nboun;
    int *boun            = app->boun;
    // ---- MESH
    int porder           = app->porder;
    int *nelem           = app->nelem;
    int *nbasis          = app->nbasis;
    int *nknot           = app->nknot;
    double **knotVector  = app->knotVector;
    // ---- IGA
    int nquad            = app->nquad;
    double *cquad        = app->cquad;
    double *wquad        = app->wquad;
    // ---- MPI
    int nselfIdx         = app->nselfIdx;
    int *selfIdx         = app->selfIdx;
    int nsendPart        = app->nsendPart;
    int *sendPart        = app->sendPart;
    int nsendIdx         = app->nsendIdx;
    int *sendIdx         = app->sendIdx;
    int *sendPtr         = app->sendPtr;
    int nrecvPart        = app->nrecvPart;
    int *recvPart        = app->recvPart;
    int nrecvIdx         = app->nrecvIdx;
    int *recvIdx         = app->recvIdx;
    int *recvPtr         = app->recvPtr;
    int *globalIdx       = app->globalIdx;
    /* ================ assemble Ui ================ */
    double *Ui           = (double*) malloc((nselfIdx+nrecvIdx)*sizeof(double));

    const double *Ui_self;
    ierr = VecGetArrayRead(Ui_,&Ui_self);CHKERRQ(ierr);
    for (int i=0; i<nselfIdx; i++)
    {
        Ui[selfIdx[i]]=Ui_self[i];
    }
    ierr = VecRestoreArrayRead(Ui_,&Ui_self);CHKERRQ(ierr);

    MPI_Status status;
    int ierror;
    double *U_send=(double*) malloc(nsendIdx*sizeof(double));
    double *U_recv=(double*) malloc(nrecvIdx*sizeof(double));
    for (int i=0; i<nsendIdx; i++) { U_send[i]=Ui[sendIdx[i]]; }
    for (int isendPart=0; isendPart<nsendPart; isendPart++) 
    { ierror=MPI_Send(U_send+sendPtr[isendPart],sendPtr[isendPart+1]-sendPtr[isendPart],MPI_DOUBLE,sendPart[isendPart],22,MPI_COMM_WORLD); assert(ierror==MPI_SUCCESS);}
    for (int irecvPart=0; irecvPart<nrecvPart; irecvPart++) 
    { ierror=MPI_Recv(U_recv+recvPtr[irecvPart],recvPtr[irecvPart+1]-recvPtr[irecvPart],MPI_DOUBLE,recvPart[irecvPart],22,MPI_COMM_WORLD,&status); assert(ierror==MPI_SUCCESS);}
    for (int i=0; i<nrecvIdx; i++) { Ui[recvIdx[i]]=U_recv[i]; }
    free(U_send);
    free(U_recv);
    /* ================ assemble Residual_ ================ */
    // ---- Mesh
    int ibpe;
    int nbpe = int_pow(porder+1,3);                   // number of active basis per element
    int ibasis;                                                    // local basis id
    double X0[ndim],X1[ndim];
    // ---- PDE
    double *residual = (double*) malloc(ndof*nddim*sizeof(double));
    double ui[ndof*nddim];
    // ---- quadrature
    double xq[ndim],xi;
    double weight;
    // ---- IGA
    double N[nbpe*nddim];
    ierr = VecSet(Residual_,0.0);CHKERRQ(ierr);
    double val[ndof*nbpe];
    int    idx[ndof*nbpe];
    double temp;
    int    ia;
    //int nbasis_x_active=nelem[0]+porder;
    int nbasis_y_active=nelem[1]+porder;
    int nbasis_z_active=nelem[2]+porder;
    /* ================================================================   VOLUME INTEGRAL   ================================================================ */
    // ---- loop over elements
    for (int ielem_x=0; ielem_x<nelem[0]; ielem_x++) {
    for (int ielem_y=0; ielem_y<nelem[1]; ielem_y++) {
    for (int ielem_z=0; ielem_z<nelem[2]; ielem_z++) {
    for (ia=0; ia<ndof*nbpe; ia++) { val[ia]=0.0; }
    // ---- loop over quadrature points
    for (int iquad_x=0; iquad_x<nquad; iquad_x++) {
    for (int iquad_y=0; iquad_y<nquad; iquad_y++) {
    for (int iquad_z=0; iquad_z<nquad; iquad_z++) {
        // ---- evaluate quadrature coord.
        X0[0]=knotVector[0][ielem_x+porder]; X1[0]=knotVector[0][ielem_x+porder+1];
        X0[1]=knotVector[1][ielem_y+porder]; X1[1]=knotVector[1][ielem_y+porder+1];
        X0[2]=knotVector[2][ielem_z+porder]; X1[2]=knotVector[2][ielem_z+porder+1];
        xi=cquad[iquad_x];xq[0]=(-xi+1.0)*X0[0]+xi*X1[0];
        xi=cquad[iquad_y];xq[1]=(-xi+1.0)*X0[1]+xi*X1[1];
        xi=cquad[iquad_z];xq[2]=(-xi+1.0)*X0[2]+xi*X1[2];
        // ---- evaluate N
        ia=0;
        for (int ibpe_x=0; ibpe_x<porder+1; ibpe_x++){
        for (int ibpe_y=0; ibpe_y<porder+1; ibpe_y++){
        for (int ibpe_z=0; ibpe_z<porder+1; ibpe_z++){
            N[ia++]=evalN(knotVector[0],nknot[0],0,ielem_x+ibpe_x,porder,xq[0])
                   *evalN(knotVector[1],nknot[1],0,ielem_y+ibpe_y,porder,xq[1])
                   *evalN(knotVector[2],nknot[2],0,ielem_z+ibpe_z,porder,xq[2]);
            for (int JJ=0; JJ<ndim; JJ++)
            {
                N[ia++]=evalN(knotVector[0],nknot[0],(int)(JJ==0),ielem_x+ibpe_x,porder,xq[0])
                       *evalN(knotVector[1],nknot[1],(int)(JJ==1),ielem_y+ibpe_y,porder,xq[1])
                       *evalN(knotVector[2],nknot[2],(int)(JJ==2),ielem_z+ibpe_z,porder,xq[2]);
            }
            for (int JJ=0; JJ<ndim; JJ++)
            {
                for (int KK=JJ; KK<ndim; KK++)
                {
                    N[ia++]=evalN(knotVector[0],nknot[0],(int)(JJ==0)+(int)(KK==0),ielem_x+ibpe_x,porder,xq[0])
                           *evalN(knotVector[1],nknot[1],(int)(JJ==1)+(int)(KK==1),ielem_y+ibpe_y,porder,xq[1])
                           *evalN(knotVector[2],nknot[2],(int)(JJ==2)+(int)(KK==2),ielem_z+ibpe_z,porder,xq[2]);
                }
            }
        }}}
        // ---- evaluate ui
        for (ia=0; ia<nddim*ndof; ia++) { ui[ia]=0.0; }
        for (int ibpe_x=0; ibpe_x<porder+1; ibpe_x++){
        for (int ibpe_y=0; ibpe_y<porder+1; ibpe_y++){
        for (int ibpe_z=0; ibpe_z<porder+1; ibpe_z++){
            ibpe=(porder+1)*(porder+1)*ibpe_x+(porder+1)*ibpe_y+ibpe_z;
            ibasis=(nbasis_y_active*nbasis_z_active)*(ielem_x+ibpe_x)+nbasis_z_active*(ielem_y+ibpe_y)+(ielem_z+ibpe_z);
            for (int idof=0; idof<ndof; idof++)
            {
                ia=ndof*ibasis+idof;
                for (int iddim=0; iddim<nddim; iddim++)
                {
                    ui[nddim*idof+iddim]+=N[nddim*ibpe+iddim]*Ui[ia];
                }
            }
        }}}
        // ---- evaluate residual
        eval_residual(residual,ui,par_mat);

        // ---- add to val
        weight=wquad[iquad_x]*wquad[iquad_y]*wquad[iquad_z]*(X1[0]-X0[0])*(X1[1]-X0[1])*(X1[2]-X0[2]);
        for (int ibpe_x=0; ibpe_x<porder+1; ibpe_x++){
        for (int ibpe_y=0; ibpe_y<porder+1; ibpe_y++){
        for (int ibpe_z=0; ibpe_z<porder+1; ibpe_z++){
            ibpe=(porder+1)*(porder+1)*ibpe_x+(porder+1)*ibpe_y+ibpe_z;
            for (int idof=0; idof<ndof; idof++)
            {
                temp=0.0;
                for (int iddim=0; iddim<nddim; iddim++) { temp+=N[nddim*ibpe+iddim]*residual[nddim*idof+iddim]; }
                val[ndof*ibpe+idof]+=weight*temp;
            }
        }}}
    }}} // iquad
    // ---- set idx
    for (int ibpe_x=0; ibpe_x<porder+1; ibpe_x++){
    for (int ibpe_y=0; ibpe_y<porder+1; ibpe_y++){
    for (int ibpe_z=0; ibpe_z<porder+1; ibpe_z++){
        ibpe=(porder+1)*(porder+1)*ibpe_x+(porder+1)*ibpe_y+ibpe_z;
        ibasis=(nbasis_y_active*nbasis_z_active)*(ielem_x+ibpe_x)+nbasis_z_active*(ielem_y+ibpe_y)+(ielem_z+ibpe_z);
        for (int idof=0; idof<ndof; idof++)
        {
            idx[ndof*ibpe+idof]=globalIdx[ndof*ibasis+idof];
        }
    }}}
    // ---- add to Residual_
    ierr = VecSetValues(Residual_,ndof*nbpe,idx,val,ADD_VALUES);CHKERRQ(ierr);
    }}} // ielem

    /* ================================================================ BOUNDARY CONDITIONS ================================================================ */
    int ax_,ay_,az_;
    int nbasis_[ndim],ibasis_z_0,ibasis_z_1;
    /* ----------------------------------------------------------------     Neumann BCs     ---------------------------------------------------------------- */
    double traction;
    double X0_[ndim-1],X1_[ndim-1];
    int    nelem_[ndim],nknot_[ndim];
    double *knotVector_[ndim];
    double N_[2*int_pow(porder+1,2)];
    double val_[2*int_pow(porder+1,2)];
    int    idx_[2*int_pow(porder+1,2)];
    double *xq_,*yq_,*zq_;
    for (int iboun=0; iboun<nboun; iboun++)
    { 
        // ---- map local -> global
        switch (boun[iboun]/2)
        {
        case 0: //  YZ-surface: x->yglobal, y->zglobal, z->xglobal
            nelem_[0]=nelem[1]; nknot_[0]=nknot[1]; knotVector_[0]=knotVector[1];
            nelem_[1]=nelem[2]; nknot_[1]=nknot[2]; knotVector_[1]=knotVector[2];
            nelem_[2]=nelem[0]; nknot_[2]=nknot[0]; knotVector_[2]=knotVector[0];
            xq_=xq+1;
            yq_=xq+2;
            zq_=xq+0;
            ax_=nbasis_z_active;
            ay_=1;
            az_=nbasis_y_active*nbasis_z_active;
            break;
        case 1: // XZ-surface: x->zglobal, y->xglobal, z->yglobal
            nelem_[0]=nelem[0]; nknot_[0]=nknot[0]; knotVector_[0]=knotVector[0];
            nelem_[1]=nelem[2]; nknot_[1]=nknot[2]; knotVector_[1]=knotVector[2];
            nelem_[2]=nelem[1]; nknot_[2]=nknot[1]; knotVector_[2]=knotVector[1];
            xq_=xq+0;
            yq_=xq+2;
            zq_=xq+1;
            ax_=nbasis_y_active*nbasis_z_active;
            ay_=1;
            az_=nbasis_z_active;
            break;
        case 2: // XY-surface: x->xglobal, y->yglobal, z->zglobal
            nelem_[0]=nelem[0]; nknot_[0]=nknot[0]; knotVector_[0]=knotVector[0];
            nelem_[1]=nelem[1]; nknot_[1]=nknot[1]; knotVector_[1]=knotVector[1];
            nelem_[2]=nelem[2]; nknot_[2]=nknot[2]; knotVector_[2]=knotVector[2];
            xq_=xq+0;
            yq_=xq+1;
            zq_=xq+2;
            ax_=nbasis_y_active*nbasis_z_active;
            ay_=nbasis_z_active;
            az_=1;
            break;
        default:
            exit(0);
        }
        switch (boun[iboun]%2)
        {
        case 0:
            *zq_=knotVector_[2][0];
            ibasis_z_0=0;
            ibasis_z_1=1;
            break;
        case 1:
            *zq_=knotVector_[2][nknot_[2]-1];
            ibasis_z_0=nelem_[2]+porder-1;
            ibasis_z_1=nelem_[2]+porder-1-1;
            break;
        }
        // ---- standard Neumann
        for (int idof=0; idof<ndof; idof++)
        {
            if (bc_type[(2*ndof)*boun[iboun]+0*ndof+idof]==1)
            {
                for (int ielem_x_=0; ielem_x_<nelem_[0]; ielem_x_++) {
                for (int ielem_y_=0; ielem_y_<nelem_[1]; ielem_y_++) {
                    X0_[0]=knotVector_[0][ielem_x_+porder]; X1_[0]=knotVector_[0][ielem_x_+porder+1];
                    X0_[1]=knotVector_[1][ielem_y_+porder]; X1_[1]=knotVector_[1][ielem_y_+porder+1];
                    // set val_
                    for (int i=0; i<int_pow(porder+1,2); i++) { val_[i]=0.0; }
                    for (int iquad_x_=0; iquad_x_<nquad; iquad_x_++) {
                    for (int iquad_y_=0; iquad_y_<nquad; iquad_y_++) {
                        xi=cquad[iquad_x_];*xq_=(-xi+1.0)*X0_[0]+xi*X1_[0];
                        xi=cquad[iquad_y_];*yq_=(-xi+1.0)*X0_[1]+xi*X1_[1];
                        ia=0;
                        for (int ibpe_x_=0; ibpe_x_<porder+1; ibpe_x_++) {
                        for (int ibpe_y_=0; ibpe_y_<porder+1; ibpe_y_++) {
                            N_[ia++]=evalN(knotVector_[0],nknot_[0],0,ielem_x_+ibpe_x_,porder,*xq_)
                                    *evalN(knotVector_[1],nknot_[1],0,ielem_y_+ibpe_y_,porder,*yq_);
                        }}
                        eval_neumann(&traction,par_neumann,boun[iboun],0,idof,xq);
                        weight=wquad[iquad_x_]*wquad[iquad_y_]*(X1_[0]-X0_[0])*(X1_[1]-X0_[1]);
                        for (int i=0; i<int_pow(porder+1,2); i++) { val_[i]-=weight*N_[i]*traction; }
                    }}
                    // set idx_
                    ia=0;
                    for (int ibpe_x_=0; ibpe_x_<porder+1; ibpe_x_++) {
                    for (int ibpe_y_=0; ibpe_y_<porder+1; ibpe_y_++) {
                        ibasis=ax_*(ielem_x_+ibpe_x_)+ay_*(ielem_y_+ibpe_y_)+az_*ibasis_z_0; idx_[ia++]=globalIdx[ndof*ibasis+idof];
                    }}
                    // add val_ & idx_
                    ierr = VecSetValues(Residual_,int_pow(porder+1,2),idx_,val_,ADD_VALUES);
                }} // ielem
            } // if bc_type
        } // for idof
        // ---- high-order Neumann
        for (int idof=0; idof<ndof; idof++)
        {
            if (bc_type[(2*ndof)*boun[iboun]+1*ndof+idof]==1)
            {
                for (int ielem_x_=0; ielem_x_<nelem_[0]; ielem_x_++) {
                for (int ielem_y_=0; ielem_y_<nelem_[1]; ielem_y_++) {
                    X0_[0]=knotVector_[0][ielem_x_+porder]; X1_[0]=knotVector_[0][ielem_x_+porder+1];
                    X0_[1]=knotVector_[1][ielem_y_+porder]; X1_[1]=knotVector_[1][ielem_y_+porder+1];
                    // set val_
                    for (int i=0; i<2*int_pow(porder+1,2); i++) { val_[i]=0.0; }
                    for (int iquad_x_=0; iquad_x_<nquad; iquad_x_++) {
                    for (int iquad_y_=0; iquad_y_<nquad; iquad_y_++) {
                        xi=cquad[iquad_x_];*xq_=(-xi+1.0)*X0_[0]+xi*X1_[0];
                        xi=cquad[iquad_y_];*yq_=(-xi+1.0)*X0_[1]+xi*X1_[1];
                        ia=0;
                        for (int ibpe_x_=0; ibpe_x_<porder+1; ibpe_x_++) {
                        for (int ibpe_y_=0; ibpe_y_<porder+1; ibpe_y_++) {
                            N_[ia++]=evalN(knotVector_[0],nknot_[0],0,ielem_x_+ibpe_x_,porder,*xq_)
                                    *evalN(knotVector_[1],nknot_[1],0,ielem_y_+ibpe_y_,porder,*yq_)
                                    *evalN(knotVector_[2],nknot_[2],1,ibasis_z_0      ,porder,*zq_);
                            N_[ia++]=evalN(knotVector_[0],nknot_[0],0,ielem_x_+ibpe_x_,porder,*xq_)
                                    *evalN(knotVector_[1],nknot_[1],0,ielem_y_+ibpe_y_,porder,*yq_)
                                    *evalN(knotVector_[2],nknot_[2],1,ibasis_z_1      ,porder,*zq_);
                        }}
                        eval_neumann(&traction,par_neumann,boun[iboun],1,idof,xq);
                        weight=wquad[iquad_x_]*wquad[iquad_y_]*(X1_[0]-X0_[0])*(X1_[1]-X0_[1]);
                        for (int i=0; i<2*int_pow(porder+1,2); i++) { val_[i]-=weight*N_[i]*traction; }
                    }}
                    // set idx_
                    ia=0;
                    for (int ibpe_x_=0; ibpe_x_<porder+1; ibpe_x_++) {
                    for (int ibpe_y_=0; ibpe_y_<porder+1; ibpe_y_++) {
                        ibasis=ax_*(ielem_x_+ibpe_x_)+ay_*(ielem_y_+ibpe_y_)+az_*ibasis_z_0; idx_[ia++]=globalIdx[ndof*ibasis+idof];
                        ibasis=ax_*(ielem_x_+ibpe_x_)+ay_*(ielem_y_+ibpe_y_)+az_*ibasis_z_1; idx_[ia++]=globalIdx[ndof*ibasis+idof];
                    }}
                    // add val_ & idx_
                    ierr = VecSetValues(Residual_,2*int_pow(porder+1,2),idx_,val_,ADD_VALUES);
                }} // ielem
            } // if bc_type
        } // for idof
    } // for iboun

    // ----
    ierr = VecAssemblyBegin(Residual_);CHKERRQ(ierr);
    ierr = VecAssemblyEnd(Residual_);CHKERRQ(ierr);

    /* ----------------------------------------------------------------    Dirichlet BCs    ---------------------------------------------------------------- */
    int nrow_dirichlet;
    int *row_dirichlet;
    double *val_dirichlet;
    double *dirichlet;
    for (int iboun=0; iboun<nboun; iboun++)
    {
        switch (boun[iboun]/2)
        {
        case 0:
            nknot_[0]=nknot[1]; knotVector_[0]=knotVector[1]; nbasis_[0]=nbasis[1]; ax_=nbasis_z_active;
            nknot_[1]=nknot[2]; knotVector_[1]=knotVector[2]; nbasis_[1]=nbasis[2]; ay_=1;
            nknot_[2]=nknot[0]; knotVector_[2]=knotVector[0]; nbasis_[2]=nbasis[0]; az_=nbasis_y_active*nbasis_z_active;
            break;
        case 1:
            nknot_[0]=nknot[0]; knotVector_[0]=knotVector[0]; nbasis_[0]=nbasis[0]; ax_=nbasis_y_active*nbasis_z_active;
            nknot_[1]=nknot[2]; knotVector_[1]=knotVector[2]; nbasis_[1]=nbasis[2]; ay_=1; 
            nknot_[2]=nknot[1]; knotVector_[2]=knotVector[1]; nbasis_[2]=nbasis[1]; az_=nbasis_z_active;
            break;
        case 2:
            nknot_[0]=nknot[0]; knotVector_[0]=knotVector[0]; nbasis_[0]=nbasis[0]; ax_=nbasis_y_active*nbasis_z_active;
            nknot_[1]=nknot[1]; knotVector_[1]=knotVector[1]; nbasis_[1]=nbasis[1]; ay_=nbasis_z_active;
            nknot_[2]=nknot[2]; knotVector_[2]=knotVector[2]; nbasis_[2]=nbasis[2]; az_=1;
            break;
        default:
            exit(0);
        }
        switch (boun[iboun]%2)
        {
        case 0:
            ibasis_z_0=0;
            ibasis_z_1=1;
            break;
        case 1:
            ibasis_z_0=nbasis_[2]-1;
            ibasis_z_1=nbasis_[2]-2;
            break;
        }
        // standard
        for (int idof=0; idof<ndof; idof++) {
            if (bc_type[(2*ndof)*boun[iboun]+0*ndof+idof]==0) {
                nrow_dirichlet=nbasis_[0]*nbasis_[1];
                row_dirichlet = (int*) malloc(nrow_dirichlet*sizeof(int));
                val_dirichlet = (double*) malloc(nrow_dirichlet*sizeof(double));
                dirichlet     = (double*) malloc(nrow_dirichlet*sizeof(double));
                ia=0;
                for (int ibasis_x_=0; ibasis_x_<nbasis_[0]; ibasis_x_++) {
                for (int ibasis_y_=0; ibasis_y_<nbasis_[1]; ibasis_y_++) {
                    ibasis=ax_*ibasis_x_+ay_*ibasis_y_+az_*ibasis_z_0;
                    row_dirichlet[ia]=globalIdx[ndof*ibasis+idof];
                    val_dirichlet[ia]=       Ui[ndof*ibasis+idof];
                    ia++;
                }}
                appl_dirichlet(dirichlet,par_dirichlet,boun[iboun],0,idof,knotVector_,nknot_,nbasis_,ndim,porder);
                for (int i=0; i<nrow_dirichlet; i++) { val_dirichlet[i]-=dirichlet[i]; }
                ierr = VecSetValues(Residual_,nrow_dirichlet,row_dirichlet,val_dirichlet,INSERT_VALUES);
                free(row_dirichlet);
                free(val_dirichlet);
                free(dirichlet);
        }}
        // high-order
        for (int idof=0; idof<ndof; idof++) {
            if (bc_type[(2*ndof)*boun[iboun]+1*ndof+idof]==0) {
                nrow_dirichlet=nbasis_[0]*nbasis_[1];
                row_dirichlet = (int*) malloc(nrow_dirichlet*sizeof(int));
                val_dirichlet = (double*) malloc(nrow_dirichlet*sizeof(double));
                dirichlet     = (double*) malloc(nrow_dirichlet*sizeof(double));
                ia=0;
                for (int ibasis_x_=0; ibasis_x_<nbasis_[0]; ibasis_x_++) {
                for (int ibasis_y_=0; ibasis_y_<nbasis_[1]; ibasis_y_++) {
                    ibasis=ax_*ibasis_x_+ay_*ibasis_y_+az_*ibasis_z_1;
                    row_dirichlet[ia]=globalIdx[ndof*ibasis+idof];
                    val_dirichlet[ia]=       Ui[ndof*ibasis+idof];
                    ia++;
                }}
                appl_dirichlet(dirichlet,par_dirichlet,boun[iboun],1,idof,knotVector_,nknot_,nbasis_,ndim,porder);
                for (int i=0; i<nrow_dirichlet; i++) { val_dirichlet[i]-=dirichlet[i]; }
                ierr = VecSetValues(Residual_,nrow_dirichlet,row_dirichlet,val_dirichlet,INSERT_VALUES);
                free(row_dirichlet);
                free(val_dirichlet);
                free(dirichlet);
        }}
    } // for iboun

    ierr = VecAssemblyBegin(Residual_);CHKERRQ(ierr);
    ierr = VecAssemblyEnd(Residual_);CHKERRQ(ierr);
    /* ---------------------------------------------------------------- FINAL ---------------------------------------------------------------- */
    free(Ui);
    free(residual);

    return 0;
}

