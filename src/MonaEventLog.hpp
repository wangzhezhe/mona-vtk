/*=========================================================================

  Program:   Visualization Toolkit
  Module:    MonaEventLog.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   MonaEventLog
 * @brief   Class for logging and timing.
 *
 *
 * This class is wrapper around MPE event logging functions
 * (available from Argonne National Lab/Missippi State
 * University). It allows users to create events with names
 * and log them. Different log file formats can be generated
 * by changing MPE's configuration. Some of these formats are
 * binary (for examples SLOG and CLOG) and can be analyzed with
 * viewers from ANL. ALOG is particularly useful since it is
 * text based and can be processed with simple scripts.
 *
 * @sa
 * vtkTimerLog MonaController MonaCommunicator
*/

#ifndef MonaEventLog_h
#define MonaEventLog_h

#include "vtkParallelMPIModule.h" // For export macro
#include "vtkObject.h"

class VTKPARALLELMPI_EXPORT MonaEventLog : public vtkObject
{
public:
  vtkTypeMacro(MonaEventLog,vtkObject);

  /**
   * Construct a MonaEventLog with the following initial state:
   * Processes = 0, MaximumNumberOfProcesses = 0.
   */
  static MonaEventLog* New();

  /**
   * Used to initialize the underlying mpe event.
   * HAS TO BE CALLED BY ALL PROCESSES before any event
   * logging is done.
   * It takes a name and a description for the graphical
   * representation, for example, "red:vlines3". See
   * mpe documentation for details.
   * Returns 0 on MPI failure (or aborts depending on
   * MPI error handlers)
   */
  int SetDescription(const char* name, const char* desc);

  //@{
  /**
   * These methods have to be called once on all processors
   * before and after invoking any logging events.
   * The name of the logfile is given by fileName.
   * See mpe documentation for file formats.
   */
  static void InitializeLogging();
  static void FinalizeLogging(const char* fileName);
  //@}

  //@{
  /**
   * Issue start and stop events for this log entry.
   */
  void StartLogging();
  void StopLogging();
  //@}

  void PrintSelf(ostream& os, vtkIndent indent) override;

protected:

  MonaEventLog();
  ~MonaEventLog();

  static int LastEventId;
  int Active;
  int BeginId;
  int EndId;
private:
  MonaEventLog(const MonaEventLog&) = delete;
  void operator=(const MonaEventLog&) = delete;
};

#endif




