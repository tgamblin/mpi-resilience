// ===========================================================================
// Step handling
// ===========================================================================

/*!
 * An MPI Step is a monotonically increasing, per-process value that indicates
 * how far an MPI application has progressed.
 */
typedef uint64_t MPI_Step;

/*!
 * Applications call this routine to indicate that they have completed a
 * particular resilience step.
 *
 * So that we can determine where to roll back when a fault occurs.
 */
int MPI_Step_complete(MPI_Step step);

// ===========================================================================
// Step handling
// ===========================================================================

/*!
 * This is the function pointer type for the main entry point of a resilient MPI
 * implementation.  An MPI_Main_function is called by MPI_Reinit to start or
 * restart program execution.
 *
 * @param[in] argc          Number of command line arguments.
 * @param[in] argv          Vector of command line arguments.
 * @param[in] restart_step  Step to begin execution from.
 *
 * If starting for the first time, restart_step will be start_step from
 * MPI_Reinit.
 */
typedef void (*MPI_Main_function)(
  int argc,
  char **argv,
  MPI_Step restart_step);

/*!
 * MPI_Reinit marks the start of a resilient MPI program.  Caller should pass
 * command line arguments, a function to be invoked or re-invoked on resilient
 * start.
 *
 * @param[in] argc            number of command line arguments
 * @param[in] argv            Vector of command line arguments
 * @param[in] resilient_main  Start point for resilient program.
 * @param[in] start_step      Default start step for entire program.
 */
int MPI_Reinit(int argc, char **argv,
               const MPI_Main_function resilient_main,
               MPI_Step start_step);

/*!
 * Indicate an application-detected fault that should trigger cleanup and
 * recovery on all processes.
 *
 * MPI_Fault() runs a consensus protocol so that all processes agree which
 * MPI_Step they should restart from.  The default consensus protocol will agree
 * on the minimum step any process has progressed to.
 *
 * This should have the same behavior as when MPI detects a fault and triggers
 * the recovery process.
 */
int MPI_Fault();


/*!
 * Test for faults that have occurred since the last MPI call made.  This allows
 * a computation-intensive loop to periodically check for faults so that it does
 * not get too far ahead of other processes.
 *
 * If a fault is detected, then recovery will be initiated.
 */
int MPI_Fault_probe();


// ===========================================================================
// Cleanup handling
// ===========================================================================

/*!
 * Possible return codes for an MPI cleanup handler.
 */
typedef int MPI_Cleanup_code;
#define MPI_CLEANUP_ABORT   0    /*!< Cleanup failed, application aborts. */
#define MPI_CLEANUP_SUCCESS 0    /*!< Cleanup succeeded, continue rollback. */

/*!
 * An MPI_cleanup_handler cleans up application- or library-allocated resourcs
 * when an MPI fault occurs.
 *
 * Cleanup handlers follow stack semantics.  That is, they are executed in LIFO
 * order in terms of when MPI_Cleanup_handler_push() was called.
 *
 * Each cleanup handler needs to return an MPI_Cleanup_code when it is done.
 * If MPI_CLEANUP_ABORT is returned by any cleanup handler, then we consider
 * this unrecoverable, and the entire application will abort.
 *
 * When all cleanup handlers return MPI_CLEANUP_SUCCESS, the semantics of
 * "success" are up to the implementor.  For example, a numerical library might
 * register a cleanup handler that returns success, but this means that it has
 * completely reinitialized itself, all library handles held by the application
 * are invalid, and the app needs to restart.  Another library might leave some
 * deallocations to be handled by the application.
 *
 * @param[in] start_step  the MPI_Step that we're restoring to.
 */
typedef MPI_Cleanup_code (*MPI_Cleanup_handler)(MPI_Step start_step);
#define MPI_CLEANUP_HANDLER_NULL ((MPI_Cleanup_handler*) 0)

/*!
 * Push a cleanup handler onto this process's stack of cleanup handlers.  The
 * handler will be executed in LIFO order when a fault occurs, assuming it's not
 * popped before then.
 */
int MPI_Cleanup_handler_push(const MPI_Cleanup_handler handler);

/*!
 * Pop the last registered handler off the cleanup handler stack.
 * If there are no handlers remaining on the stack, then this will
 * write out MPI_CLEANUP_HANDLER_NULL.
 *
 * @param[out] handler  Where to store the popped handler.
 */
int MPI_Cleanup_handler_pop(const MPI_Cleanup_handler *handler);

/*!
 * Delete a particular cleanup handler from the handler stack.
 */
int MPI_Cleanup_handler_delete(const MPI_Cleanup_handler handler);
