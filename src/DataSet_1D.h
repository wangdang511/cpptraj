#ifndef INC_DATASET_1D_H
#define INC_DATASET_1D_H
#include "DataSet.h"
#include "CpptrajFile.h"
/// Class that all 1D scalar DataSets will inherit.
class DataSet_1D : public DataSet {
  public:
    DataSet_1D() {}
    DataSet_1D(DataSet::DataType tIn, int wIn, int pIn) : DataSet(tIn, SCALAR_1D, wIn, pIn, 1) {}
    virtual ~DataSet_1D() {}
    /// \return data from set at position as double precision.
    virtual double Dval(size_t) const = 0;
    /// \return the value of the X coordinate at position.
    virtual double Xcrd(size_t) const = 0;
    // -------------------------------------------
    double Avg()           const { return Avg( 0 ); }
    double Avg(double& sd) const { return Avg(&sd); }
    double Min() const;
    double Max() const;
    int CrossCorr(DataSet_1D const&, DataSet_1D&, int, bool, bool) const;
    double CorrCoeff(DataSet_1D const&) const;
  private:
    double Avg(double*) const;
};
#endif
