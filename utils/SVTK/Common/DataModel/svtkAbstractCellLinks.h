/*=========================================================================

  Program:   Visualization Toolkit
  Module:    svtkAbstractCellLinks.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   svtkAbstractCellLinks
 * @brief   an abstract base class for classes that build
 * topological links from points to cells
 *
 * svtkAbstractCellLinks is a family of supplemental objects to svtkCellArray and
 * svtkCellTypes, enabling fast access from points to the cells using the
 * points. svtkAbstractCellLinks is an array of links, each link representing a
 * list of cell ids using a particular point. The information provided by
 * this object can be used to determine neighbors and construct other local
 * topological information.
 *
 * @sa
 * svtkCellLinks svtkStaticCellLinks svtkStaticCellLinksTemplate
 */

#ifndef svtkAbstractCellLinks_h
#define svtkAbstractCellLinks_h

#include "svtkCommonDataModelModule.h" // For export macro
#include "svtkObject.h"

class svtkDataSet;
class svtkCellArray;
class svtkIdList;

class SVTKCOMMONDATAMODEL_EXPORT svtkAbstractCellLinks : public svtkObject
{
public:
  //@{
  /**
   * Standard type and print methods.
   */
  svtkTypeMacro(svtkAbstractCellLinks, svtkObject);
  void PrintSelf(ostream& os, svtkIndent indent) override;
  //@}

  /**
   * Build the link list array. All subclasses must implement this method.
   */
  virtual void BuildLinks(svtkDataSet* data) = 0;

  /**
   * Release memory and revert to empty state.
   */
  virtual void Initialize() = 0;

  /**
   * Reclaim any unused memory.
   */
  virtual void Squeeze() = 0;

  /**
   * Reset to a state of no entries without freeing the memory.
   */
  virtual void Reset() = 0;

  /**
   * Return the memory in kibibytes (1024 bytes) consumed by this cell links array.
   * Used to support streaming and reading/writing data. The value
   * returned is guaranteed to be greater than or equal to the memory
   * required to actually represent the data represented by this object.
   * The information returned is valid only after the pipeline has
   * been updated.
   */
  virtual unsigned long GetActualMemorySize() = 0;

  /**
   * Standard DeepCopy method.  Since this object contains no reference
   * to other objects, there is no ShallowCopy.
   */
  virtual void DeepCopy(svtkAbstractCellLinks* src) = 0;

  // Enums for cell links type. Note that the specialized type is
  // set when users do not use ComputeType() and roll their own type.
  enum CellLinksTypes
  {
    LINKS_NOT_DEFINED = 0,
    CELL_LINKS = 1,
    STATIC_CELL_LINKS_USHORT = 2,
    STATIC_CELL_LINKS_UINT = 3,
    STATIC_CELL_LINKS_IDTYPE = 4,
    STATIC_CELL_LINKS_SPECIALIZED = 5
  };

  /**
   * Based on the input (i.e., number of points, number of cells, and length
   * of connectivity array) this helper method returns the integral type to
   * use when instantiating cell link-related classes in order to properly
   * represent the data.  The return value is one of the types
   * defined in the enum CellLinksType enum defined previously. Subclasses
   * may choose to instantiate themselves with different integral types for
   * performance and/or memory reasons. This method is useful when instantiating
   * a svtkStaticCellLinksTemplate; when instantiating a svtkCellLinks the class
   * is hardwired for svtkIdType.
   */
  static int ComputeType(svtkIdType maxPtId, svtkIdType maxCellId, svtkCellArray* ca);

  /**
   * Return the type of locator (see enum above).
   */
  int GetType() { return this->Type; }

  /**
   * These methods are not virtual due to performance concerns. However,
   * subclasses of this class will implement them, using a combination of
   * static_cast<> and templating used to invoke the methods in a way that
   * the compiler can optimize.
   *
   * Get the number of cells using the point specified by ptId:
   *    svtkIdType GetNcells(svtkIdType ptId)
   *
   * Return a list of cell ids using the point.
   *    TIds *GetCells(svtkIdType ptId)
   */

  //@{
  /**
   * Force sequential processing (i.e. single thread) of the link building
   * process. By default, sequential processing is off. Note this flag only
   * applies if the class has been compiled with SVTK_SMP_IMPLEMENTATION_TYPE
   * set to something other than Sequential. (If set to Sequential, then the
   * filter always runs in serial mode.) This flag is typically used for
   * benchmarking purposes.
   */
  svtkSetMacro(SequentialProcessing, bool);
  svtkGetMacro(SequentialProcessing, bool);
  svtkBooleanMacro(SequentialProcessing, bool);
  //@}

protected:
  svtkAbstractCellLinks();
  ~svtkAbstractCellLinks() override;

  bool SequentialProcessing; // control whether to thread or not
  int Type;                  // derived classes set this instance variable when constructed

private:
  svtkAbstractCellLinks(const svtkAbstractCellLinks&) = delete;
  void operator=(const svtkAbstractCellLinks&) = delete;
};

#endif