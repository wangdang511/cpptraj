#ifndef INC_CLUSTER_HIERAGGLO_H
#define INC_CLUSTER_HIERAGGLO_H
#include "ClusterList.h"
class Cluster_HierAgglo : public ClusterList {
  public:
    /// Type of distance calculation between clusters.
    enum LINKAGETYPE  { SINGLELINK = 0, AVERAGELINK, COMPLETELINK };
    Cluster_HierAgglo();

    int SetupCluster(ArgList&);
    void ClusteringInfo();
    int Cluster();
  private:
    int nclusters_;       ///< Target # of clusters.
    double epsilon_;      ///< Once the min distance between clusters is > epsilon, stop.
    LINKAGETYPE linkage_; ///< Cluster Linkage type.

    void InitializeClusterDistances();
    int MergeClosest();
    void calcMinDist(cluster_it&);
    void calcMaxDist(cluster_it&);
    void calcAvgDist(cluster_it&);
};
#endif