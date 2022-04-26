/*=========================================================================

  Program:   Visualization Toolkit
  Module:    svtkLine.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "svtkLine.h"

#include "svtkCellArray.h"
#include "svtkCellData.h"
#include "svtkIncrementalPointLocator.h"
#include "svtkMath.h"
#include "svtkObjectFactory.h"
#include "svtkPointData.h"
#include "svtkPoints.h"

svtkStandardNewMacro(svtkLine);

//----------------------------------------------------------------------------
// Construct the line with two points.
svtkLine::svtkLine()
{
  this->Points->SetNumberOfPoints(2);
  this->PointIds->SetNumberOfIds(2);
  for (int i = 0; i < 2; i++)
  {
    this->Points->SetPoint(i, 0.0, 0.0, 0.0);
    this->PointIds->SetId(i, 0);
  }
}

//----------------------------------------------------------------------------
static const int SVTK_NO_INTERSECTION = 0;
static const int SVTK_YES_INTERSECTION = 2;
static const int SVTK_ON_LINE = 3;

int svtkLine::EvaluatePosition(const double x[3], double closestPoint[3], int& subId,
  double pcoords[3], double& dist2, double weights[])
{
  double a1[3], a2[3];

  subId = 0;
  pcoords[0] = pcoords[1] = pcoords[2] = 0.0;

  this->Points->GetPoint(0, a1);
  this->Points->GetPoint(1, a2);

  // DistanceToLine sets pcoords[0] to a value t
  dist2 = this->DistanceToLine(x, a1, a2, pcoords[0], closestPoint);

  // pcoords[0] == t, need weights to be 1-t and t
  weights[0] = 1.0 - pcoords[0];
  weights[1] = pcoords[0];

  if (pcoords[0] < 0.0 || pcoords[0] > 1.0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

//----------------------------------------------------------------------------
void svtkLine::EvaluateLocation(
  int& svtkNotUsed(subId), const double pcoords[3], double x[3], double* weights)
{
  int i;
  double a1[3], a2[3];
  this->Points->GetPoint(0, a1);
  this->Points->GetPoint(1, a2);

  for (i = 0; i < 3; i++)
  {
    x[i] = a1[i] + pcoords[0] * (a2[i] - a1[i]);
  }

  weights[0] = 1.0 - pcoords[0];
  weights[1] = pcoords[0];
}

//----------------------------------------------------------------------------
// Performs intersection of the projection of two finite 3D lines onto a 2D
// plane. An intersection is found if the projection of the two lines onto
// the plane perpendicular to the cross product of the two lines intersect.
// The parameters (u,v) are the parametric coordinates of the lines at the
// position of closest approach.
int svtkLine::Intersection(const double a1[3], const double a2[3], const double b1[3],
  const double b2[3], double& u, double& v)
{
  double a21[3], b21[3], b1a1[3];
  double c[2];
  double *A[2], row1[2], row2[2];

  //  Initialize
  u = v = 0.0;

  //   Determine line vectors.
  a21[0] = a2[0] - a1[0];
  b21[0] = b2[0] - b1[0];
  b1a1[0] = b1[0] - a1[0];
  a21[1] = a2[1] - a1[1];
  b21[1] = b2[1] - b1[1];
  b1a1[1] = b1[1] - a1[1];
  a21[2] = a2[2] - a1[2];
  b21[2] = b2[2] - b1[2];
  b1a1[2] = b1[2] - a1[2];

  //   Compute the system (least squares) matrix.
  A[0] = row1;
  A[1] = row2;
  row1[0] = svtkMath::Dot(a21, a21);
  row1[1] = -svtkMath::Dot(a21, b21);
  row2[0] = row1[1];
  row2[1] = svtkMath::Dot(b21, b21);

  //   Compute the least squares system constant term.
  c[0] = svtkMath::Dot(a21, b1a1);
  c[1] = -svtkMath::Dot(b21, b1a1);

  //  Solve the system of equations
  if (svtkMath::SolveLinearSystem(A, c, 2) == 0)
  {
    // The lines are colinear. Therefore, one of the four endpoints is the
    // point of closest approach
    double minDist = SVTK_DOUBLE_MAX;
    const double* p[4] = { a1, a2, b1, b2 };
    const double* l1[4] = { b1, b1, a1, a1 };
    const double* l2[4] = { b2, b2, a2, a2 };
    double* uv1[4] = { &v, &v, &u, &u };
    double* uv2[4] = { &u, &u, &v, &v };
    double t = 0;
    for (unsigned i = 0; i < 4; i++)
    {
      double dist = svtkLine::DistanceToLine(p[i], l1[i], l2[i], t);
      if (dist < minDist)
      {
        minDist = dist;
        *(uv1[i]) = t;
        *(uv2[i]) = static_cast<double>(i % 2); // the corresponding extremum
      }
    }
    return SVTK_ON_LINE;
  }
  else
  {
    u = c[0];
    v = c[1];
  }

  //  Check parametric coordinates for intersection.
  if ((0.0 <= u) && (u <= 1.0) && (0.0 <= v) && (v <= 1.0))
  {
    return SVTK_YES_INTERSECTION;
  }
  else
  {
    return SVTK_NO_INTERSECTION;
  }
}

//----------------------------------------------------------------------------
int svtkLine::Intersection3D(
  double a1[3], double a2[3], double b1[3], double b2[3], double& u, double& v)
{
  // Description:
  // Performs intersection of two finite 3D lines. An intersection is found if
  // the projection of the two lines onto the plane perpendicular to the cross
  // product of the two lines intersect, and if the distance between the
  // closest points of approach are within a relative tolerance. The parameters
  // (u,v) are the parametric coordinates of the lines at the position of
  // closest approach.
  int projectedIntersection = svtkLine::Intersection(a1, a2, b1, b2, u, v);

  if (projectedIntersection == SVTK_YES_INTERSECTION)
  {
    double a_i, b_i;
    double lenA = 0.;
    double lenB = 0.;
    double dist = 0.;
    for (unsigned i = 0; i < 3; i++)
    {
      a_i = a1[i] + (a2[i] - a1[i]) * u;
      b_i = b1[i] + (b2[i] - b1[i]) * v;
      lenA += (a2[i] - a1[i]) * (a2[i] - a1[i]);
      lenB += (b2[i] - b1[i]) * (b2[i] - b1[i]);
      dist += (a_i - b_i) * (a_i - b_i);
    }
    if (dist > 1.e-6 * (lenA > lenB ? lenA : lenB))
      return SVTK_NO_INTERSECTION;
  }

  return projectedIntersection;
}

//----------------------------------------------------------------------------
int svtkLine::CellBoundary(int svtkNotUsed(subId), const double pcoords[3], svtkIdList* pts)
{
  pts->SetNumberOfIds(1);

  if (pcoords[0] >= 0.5)
  {
    pts->SetId(0, this->PointIds->GetId(1));
    if (pcoords[0] > 1.0)
    {
      return 0;
    }
    else
    {
      return 1;
    }
  }
  else
  {
    pts->SetId(0, this->PointIds->GetId(0));
    if (pcoords[0] < 0.0)
    {
      return 0;
    }
    else
    {
      return 1;
    }
  }
}

//----------------------------------------------------------------------------
//
// marching lines case table
//
typedef int VERT_LIST;

typedef struct
{
  VERT_LIST verts[2];
} VERT_CASES;

static VERT_CASES vertCases[4] = {
  { { -1, -1 } },
  { { 1, 0 } },
  { { 0, 1 } },
  { { -1, -1 } },
};

//----------------------------------------------------------------------------
void svtkLine::Contour(double value, svtkDataArray* cellScalars, svtkIncrementalPointLocator* locator,
  svtkCellArray* verts, svtkCellArray* svtkNotUsed(lines), svtkCellArray* svtkNotUsed(polys),
  svtkPointData* inPd, svtkPointData* outPd, svtkCellData* inCd, svtkIdType cellId, svtkCellData* outCd)
{
  static const int CASE_MASK[2] = { 1, 2 };
  int index, i, newCellId;
  VERT_CASES* vertCase;
  VERT_LIST* vert;
  double t, x[3], x1[3], x2[3];
  svtkIdType pts[1];

  //
  // Build the case table
  //
  for (i = 0, index = 0; i < 2; i++)
  {
    if (cellScalars->GetComponent(i, 0) >= value)
    {
      index |= CASE_MASK[i];
    }
  }

  vertCase = vertCases + index;
  vert = vertCase->verts;

  if (vert[0] > -1)
  {
    t = (value - cellScalars->GetComponent(vert[0], 0)) /
      (cellScalars->GetComponent(vert[1], 0) - cellScalars->GetComponent(vert[0], 0));
    this->Points->GetPoint(vert[0], x1);
    this->Points->GetPoint(vert[1], x2);
    for (i = 0; i < 3; i++)
    {
      x[i] = x1[i] + t * (x2[i] - x1[i]);
    }

    if (locator->InsertUniquePoint(x, pts[0]))
    {
      if (outPd)
      {
        svtkIdType p1 = this->PointIds->GetId(vert[0]);
        svtkIdType p2 = this->PointIds->GetId(vert[1]);
        outPd->InterpolateEdge(inPd, pts[0], p1, p2, t);
      }
    }
    newCellId = verts->InsertNextCell(1, pts);
    if (outCd)
    {
      outCd->CopyData(inCd, cellId, newCellId);
    }
  }
}

//----------------------------------------------------------------------------
double svtkLine::DistanceBetweenLines(double l0[3], double l1[3], // line 1
  double m0[3], double m1[3],                                    // line 2
  double closestPt1[3], double closestPt2[3],                    // closest points
  double& t1, double& t2) // parametric coords of the closest points
{
  // Part of this function was adapted from "GeometryAlgorithms.com"
  //
  // Copyright 2001, softSurfer (www.softsurfer.com)
  // This code may be freely used and modified for any purpose
  // providing that this copyright notice is included with it.
  // SoftSurfer makes no warranty for this code, and cannot be held
  // liable for any real or imagined damage resulting from its use.
  // Users of this code must verify correctness for their application.

  const double u[3] = { l1[0] - l0[0], l1[1] - l0[1], l1[2] - l0[2] };
  const double v[3] = { m1[0] - m0[0], m1[1] - m0[1], m1[2] - m0[2] };
  const double w[3] = { l0[0] - m0[0], l0[1] - m0[1], l0[2] - m0[2] };
  const double a = svtkMath::Dot(u, u);
  const double b = svtkMath::Dot(u, v);
  const double c = svtkMath::Dot(v, v); // always >= 0
  const double d = svtkMath::Dot(u, w);
  const double e = svtkMath::Dot(v, w);
  const double D = a * c - b * b; // always >= 0

  // compute the line parameters of the two closest points
  if (D < 1e-6)
  { // the lines are almost parallel
    t1 = 0.0;
    t2 = (b > c ? d / b : e / c); // use the largest denominator
  }
  else
  {
    t1 = (b * e - c * d) / D;
    t2 = (a * e - b * d) / D;
  }

  for (unsigned int i = 0; i < 3; i++)
  {
    closestPt1[i] = l0[i] + t1 * u[i];
    closestPt2[i] = m0[i] + t2 * v[i];
  }

  // Return the distance squared between the lines =
  // the mag squared of the distance between the two closest points
  // = L1(t1) - L2(t2)
  return svtkMath::Distance2BetweenPoints(closestPt1, closestPt2);
}

//----------------------------------------------------------------------------
double svtkLine::DistanceBetweenLineSegments(double l0[3], double l1[3], // line segment 1
  double m0[3], double m1[3],                                           // line segment 2
  double closestPt1[3], double closestPt2[3],                           // closest points
  double& t1, double& t2)                                               // parametric coords
                                                                        // of the closest points
{
  // Part of this function was adapted from "GeometryAlgorithms.com"
  //
  // Copyright 2001, softSurfer (www.softsurfer.com)
  // This code may be freely used and modified for any purpose
  // providing that this copyright notice is included with it.
  // SoftSurfer makes no warranty for this code, and cannot be held
  // liable for any real or imagined damage resulting from its use.
  // Users of this code must verify correctness for their application.

  const double u[3] = { l1[0] - l0[0], l1[1] - l0[1], l1[2] - l0[2] };
  const double v[3] = { m1[0] - m0[0], m1[1] - m0[1], m1[2] - m0[2] };
  const double w[3] = { l0[0] - m0[0], l0[1] - m0[1], l0[2] - m0[2] };
  const double a = svtkMath::Dot(u, u);
  const double b = svtkMath::Dot(u, v);
  const double c = svtkMath::Dot(v, v); // always >= 0
  const double d = svtkMath::Dot(u, w);
  const double e = svtkMath::Dot(v, w);
  const double D = a * c - b * b; // always >= 0
  double sN, sD = D;              // sc = sN / sD, default sD = D >= 0
  double tN, tD = D;              // tc = tN / tD, default tD = D >= 0

  // compute the line parameters of the two closest points

  if (D < 1e-6)
  {
    // The lines are colinear. Therefore, one of the four endpoints is the
    // point of closest approach
    double minDist = SVTK_DOUBLE_MAX;
    double* p[4] = { l0, l1, m0, m1 };
    double* a1[4] = { m0, m0, l0, l0 };
    double* a2[4] = { m1, m1, l1, l1 };
    double* uv1[4] = { &t2, &t2, &t1, &t1 };
    double* uv2[4] = { &t1, &t1, &t2, &t2 };
    double* pn1[4] = { closestPt2, closestPt2, closestPt1, closestPt1 };
    double* pn2[4] = { closestPt1, closestPt1, closestPt2, closestPt2 };
    double dist, pn_[3];
    for (unsigned i = 0; i < 4; i++)
    {
      double t = 0;
      dist = svtkLine::DistanceToLine(p[i], a1[i], a2[i], t, pn_);
      if (dist < minDist)
      {
        minDist = dist;
        *(uv1[i]) = (t < 0. ? 0. : (t > 1. ? 1. : t));
        *(uv2[i]) = static_cast<double>(i % 2); // the corresponding extremum
        for (unsigned j = 0; j < 3; j++)
        {
          pn1[i][j] = pn_[j];
          pn2[i][j] = p[i][j];
        }
      }
    }
    return minDist;
  }

  // The lines aren't parallel.

  else
  { // get the closest points on the infinite lines
    sN = (b * e - c * d);
    tN = (a * e - b * d);
    if (sN < 0.0)
    { // sc < 0 => the s=0 edge is visible
      sN = 0.0;
      tN = e;
      tD = c;
    }
    else if (sN > sD)
    { // sc > 1 => the s=1 edge is visible
      sN = sD;
      tN = e + b;
      tD = c;
    }
  }

  if (tN < 0.0)
  { // tc < 0 => the t=0 edge is visible
    tN = 0.0;

    // recompute sc for this edge
    if (-d < 0.0)
    {
      sN = 0.0;
    }
    else if (-d > a)
    {
      sN = sD;
    }
    else
    {
      sN = -d;
      sD = a;
    }
  }
  else if (tN > tD)
  { // tc > 1 => the t=1 edge is visible
    tN = tD;

    // recompute sc for this edge
    if ((-d + b) < 0.0)
    {
      sN = 0;
    }
    else if ((-d + b) > a)
    {
      sN = sD;
    }
    else
    {
      sN = (-d + b);
      sD = a;
    }
  }

  // finally do the division to get sc and tc
  t1 = (fabs(sN) < 1e-6 ? 0.0 : sN / sD);
  t2 = (fabs(tN) < 1e-6 ? 0.0 : tN / tD);

  // Closest Point on segment1 = S1(t1) = l0 + t1*u
  // Closest Point on segment2 = S1(t2) = m0 + t2*v

  for (unsigned int i = 0; i < 3; i++)
  {
    closestPt1[i] = l0[i] + t1 * u[i];
    closestPt2[i] = m0[i] + t2 * v[i];
  }

  // Return the distance squared between the lines =
  // the mag squared of the distance between the two closest points
  // = S1(t1) - S2(t2)
  return svtkMath::Distance2BetweenPoints(closestPt1, closestPt2);
}

//----------------------------------------------------------------------------
// Compute distance to finite line. Returns parametric coordinate t
// and point location on line.
double svtkLine::DistanceToLine(
  const double x[3], const double p1[3], const double p2[3], double& t, double closestPoint[3])
{
  const double* closest = nullptr;
  //
  //   Determine appropriate vectors
  //
  double p21[3] = { p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2] };

  //
  //   Get parametric location
  //
  double num = p21[0] * (x[0] - p1[0]) + p21[1] * (x[1] - p1[1]) + p21[2] * (x[2] - p1[2]);
  if (num == 0.0)
  {
    t = 0;
    closest = p1;
  }
  else
  {
    double denom = svtkMath::Dot(p21, p21);

    // trying to avoid an expensive fabs
    double tolerance = SVTK_TOL * num;
    if (tolerance < 0.0)
    {
      tolerance = -tolerance;
    }
    if (denom < tolerance) // numerically bad!
    {
      if (num > 0)
      {
        closest = p2;
        t = SVTK_DOUBLE_MAX;
      }
      else
      {
        closest = p1;
        t = SVTK_DOUBLE_MIN;
      }
    }
    //
    // If parametric coordinate is within 0<=p<=1, then the point is closest to
    // the line.  Otherwise, it's closest to a point at the end of the line.
    //
    else if ((t = num / denom) < 0.0)
    {
      closest = p1;
    }
    else if (t > 1.0)
    {
      closest = p2;
    }
    else
    {
      closest = p21;
      p21[0] = p1[0] + t * p21[0];
      p21[1] = p1[1] + t * p21[1];
      p21[2] = p1[2] + t * p21[2];
    }
  }

  if (closestPoint)
  {
    closestPoint[0] = closest[0];
    closestPoint[1] = closest[1];
    closestPoint[2] = closest[2];
  }
  return svtkMath::Distance2BetweenPoints(closest, x);
}

//----------------------------------------------------------------------------
//
// Determine the distance of the current vertex to the edge defined by
// the vertices provided.  Returns distance squared. Note: line is assumed
// infinite in extent.
//
double svtkLine::DistanceToLine(const double x[3], const double p1[3], const double p2[3])
{
  int i;
  double np1[3], p1p2[3], proj, den;

  for (i = 0; i < 3; i++)
  {
    np1[i] = x[i] - p1[i];
    p1p2[i] = p1[i] - p2[i];
  }

  if ((den = svtkMath::Norm(p1p2)) != 0.0)
  {
    for (i = 0; i < 3; i++)
    {
      p1p2[i] /= den;
    }
  }
  else
  {
    return svtkMath::Dot(np1, np1);
  }

  proj = svtkMath::Dot(np1, p1p2);

  return (svtkMath::Dot(np1, np1) - proj * proj);
}

//----------------------------------------------------------------------------
// Line-line intersection. Intersection has to occur within [0,1] parametric
// coordinates and with specified tolerance.
int svtkLine::IntersectWithLine(const double p1[3], const double p2[3], double tol, double& t,
  double x[3], double pcoords[3], int& subId)
{
  double a1[3], a2[3];
  double projXYZ[3];
  int i;

  subId = 0;
  pcoords[1] = pcoords[2] = 0.0;

  this->Points->GetPoint(0, a1);
  this->Points->GetPoint(1, a2);

  if (this->Intersection(p1, p2, a1, a2, t, pcoords[0]) == SVTK_YES_INTERSECTION)
  {
    // make sure we are within tolerance
    for (i = 0; i < 3; i++)
    {
      x[i] = a1[i] + pcoords[0] * (a2[i] - a1[i]);
      projXYZ[i] = p1[i] + t * (p2[i] - p1[i]);
    }
    if (svtkMath::Distance2BetweenPoints(x, projXYZ) <= tol * tol)
    {
      return 1;
    }
    else
    {
      return 0;
    }
  }

  else // check to see if it lies within tolerance
  {
    // one of the parametric coords must be outside 0-1
    if (t < 0.0)
    {
      t = 0.0;
      if (svtkLine::DistanceToLine(p1, a1, a2, pcoords[0], x) <= tol * tol)
      {
        return 1;
      }
      else
      {
        return 0;
      }
    }
    if (t > 1.0)
    {
      t = 1.0;
      if (svtkLine::DistanceToLine(p2, a1, a2, pcoords[0], x) <= tol * tol)
      {
        return 1;
      }
      else
      {
        return 0;
      }
    }
    if (pcoords[0] < 0.0)
    {
      pcoords[0] = 0.0;
      if (svtkLine::DistanceToLine(a1, p1, p2, t, x) <= tol * tol)
      {
        return 1;
      }
      else
      {
        return 0;
      }
    }
    if (pcoords[0] > 1.0)
    {
      pcoords[0] = 1.0;
      if (svtkLine::DistanceToLine(a2, p1, p2, t, x) <= tol * tol)
      {
        return 1;
      }
      else
      {
        return 0;
      }
    }
  }
  return 0;
}

//----------------------------------------------------------------------------
int svtkLine::Triangulate(int svtkNotUsed(index), svtkIdList* ptIds, svtkPoints* pts)
{
  pts->Reset();
  ptIds->Reset();

  ptIds->InsertId(0, this->PointIds->GetId(0));
  pts->InsertPoint(0, this->Points->GetPoint(0));

  ptIds->InsertId(1, this->PointIds->GetId(1));
  pts->InsertPoint(1, this->Points->GetPoint(1));

  return 1;
}

//----------------------------------------------------------------------------
void svtkLine::Derivatives(int svtkNotUsed(subId), const double svtkNotUsed(pcoords)[3],
  const double* values, int dim, double* derivs)
{
  double x0[3], x1[3], deltaX[3];
  int i, j;

  this->Points->GetPoint(0, x0);
  this->Points->GetPoint(1, x1);

  for (i = 0; i < 3; i++)
  {
    deltaX[i] = x1[i] - x0[i];
  }
  for (i = 0; i < dim; i++)
  {
    for (j = 0; j < 3; j++)
    {
      if (deltaX[j] != 0)
      {
        derivs[3 * i + j] = (values[i + dim] - values[i]) / deltaX[j];
      }
      else
      {
        derivs[3 * i + j] = 0;
      }
    }
  }
}

//----------------------------------------------------------------------------
// support line clipping
namespace
{ // required so we don't violate ODR
typedef int LINE_LIST;
typedef struct
{
  LINE_LIST lines[2];
} LINE_CASES;

static LINE_CASES lineCases[] = {
  { { -1, -1 } },   // 0
  { { 100, 1 } },   // 1
  { { 0, 101 } },   // 2
  { { 100, 101 } }, // 3
};
}

// Clip this line using scalar value provided. Like contouring, except
// that it cuts the line to produce other lines.
void svtkLine::Clip(double value, svtkDataArray* cellScalars, svtkIncrementalPointLocator* locator,
  svtkCellArray* lines, svtkPointData* inPd, svtkPointData* outPd, svtkCellData* inCd, svtkIdType cellId,
  svtkCellData* outCd, int insideOut)
{
  static const int CASE_MASK[3] = { 1, 2 };
  LINE_CASES* lineCase;
  int i, j, index, *vert, newCellId;
  svtkIdType pts[3];
  int vertexId;
  double t, x1[3], x2[3], x[3];

  // Build the case table
  if (insideOut)
  {
    for (i = 0, index = 0; i < 2; i++)
    {
      if (cellScalars->GetComponent(i, 0) <= value)
      {
        index |= CASE_MASK[i];
      }
    }
  }
  else
  {
    for (i = 0, index = 0; i < 2; i++)
    {
      if (cellScalars->GetComponent(i, 0) > value)
      {
        index |= CASE_MASK[i];
      }
    }
  }

  // Select the case based on the index and get the list of lines for this case
  lineCase = lineCases + index;
  vert = lineCase->lines;

  // generate clipped line
  if (vert[0] > -1)
  {
    for (i = 0; i < 2; i++) // insert line
    {
      // vertex exists, and need not be interpolated
      if (vert[i] >= 100)
      {
        vertexId = vert[i] - 100;
        this->Points->GetPoint(vertexId, x);
        if (locator->InsertUniquePoint(x, pts[i]))
        {
          outPd->CopyData(inPd, this->PointIds->GetId(vertexId), pts[i]);
        }
      }

      else // new vertex, interpolate
      {
        t = (value - cellScalars->GetComponent(0, 0)) /
          (cellScalars->GetComponent(1, 0) - cellScalars->GetComponent(0, 0));

        this->Points->GetPoint(0, x1);
        this->Points->GetPoint(1, x2);
        for (j = 0; j < 3; j++)
        {
          x[j] = x1[j] + t * (x2[j] - x1[j]);
        }

        if (locator->InsertUniquePoint(x, pts[i]))
        {
          svtkIdType p1 = this->PointIds->GetId(0);
          svtkIdType p2 = this->PointIds->GetId(1);
          outPd->InterpolateEdge(inPd, pts[i], p1, p2, t);
        }
      }
    }
    // check for degenerate lines
    if (pts[0] != pts[1])
    {
      newCellId = lines->InsertNextCell(2, pts);
      outCd->CopyData(inCd, cellId, newCellId);
    }
  }
}

//----------------------------------------------------------------------------
//
// Compute interpolation functions
//
void svtkLine::InterpolationFunctions(const double pcoords[3], double weights[2])
{
  weights[0] = 1.0 - pcoords[0];
  weights[1] = pcoords[0];
}

//----------------------------------------------------------------------------
void svtkLine::InterpolationDerivs(const double svtkNotUsed(pcoords)[3], double derivs[2])
{
  derivs[0] = -1.0;
  derivs[1] = 1.0;
}

//----------------------------------------------------------------------------
static double svtkLineCellPCoords[6] = { 0.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
double* svtkLine::GetParametricCoords()
{
  return svtkLineCellPCoords;
}

//----------------------------------------------------------------------------
void svtkLine::PrintSelf(ostream& os, svtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}