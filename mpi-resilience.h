// ===========================================================================
// MPI process start states
// ===========================================================================

/*!
 * An MPI_Start_state tells you under what circumstances a process was
 * initialized.
 */
typedef enum {
  MPI_START_NEW,        //!< Fresh process with no faults (first start)
  MPI_START_RESTARTED,  //!< Process restarted due to a fault.
  MPI_START_ADDED,      //!< Process is new but was added to existing job.
} MPI_Start_state;

// ===========================================================================
// Initialization & reinitialization
// ===========================================================================

/*!
 * This is the function pointer type for the main entry point of a resilient
 * MPI implementation.  An MPI_Restart_point is called by MPI_Reinit to start
 * or restart program execution.
 *
 * @param[in] argc          Number of command line arguments.
 * @param[in] argv          Vector of command line arguments.
 * @param[in] start_state   How the process started up.
 *
 * If starting for the first time, start_state will be MPI_START_NEW.  If
 * restarting due to a fault, start_state will be MPI_START_RESTARTED.  If
 * this process was added to replace a failed process in another job,
 * start_state will be MPI_START_NEW.
 *
 * Some guarantees on rank order:
 *
 * 1. If the size of MPI_COMM_WORLD is the SAME or larger than it was before a
 *    fault, then ranks of restarted processes will be the same as before the
 *    fault, and added processes' ranks will be the same as those that failed.
 *
 * 2. If the size of MPI_COMM_WORLD is smaller than it was before a fault,
 *    then there are no guarantees on rank order.
 *
 */
typedef void (*MPI_Restart_point)(int argc, char **argv,
                                  MPI_Start_state start_state);

/*!
 * MPI_Reinit marks the start of a resilient MPI program.  Caller should pass
 * command line arguments, and a function to be invoked each time this process
 * restarts.  The MPI_Restart_point is passed the same arguments each time,
 * but will be passed a different MPI_Start_state depending on how the fault
 * occurred.
 *
 * @param[in] argc            number of command line arguments
 * @param[in] argv            Vector of command line arguments
 * @param[in] restart_point   Entry point for restarts.
 */
int MPI_Reinit(int argc, char **argv,
               const MPI_Restart_point restart_point);


// ===========================================================================
// Sending fault notification
// ===========================================================================

/*!
 * Indicate an application-detected fault that should trigger cleanup and
 * recovery on all processes.
 *
 * This should have the same behavior as when MPI detects a fault and triggers
 * the recovery process itself.
 */
int MPI_Fault();


// ===========================================================================
// Cleanup handling
// ===========================================================================

/*!
 * Possible return codes for an MPI cleanup handler.
 */
typedef enum {
  MPI_CLEANUP_ABORT,    //!< Cleanup failed, application aborts.
  MPI_CLEANUP_SUCCESS,  //!< Cleanup succeeded, continue rollback.
} MPI_Cleanup_code;

/*!
 * An MPI_cleanup_handler cleans up application- or library-allocated resourcs
 * when an MPI fault occurs.
 *
 * Cleanup handlers follow stack semantics.  A user can push new cleanup
 * handlers onto the stack as she allocates resources, and if the resources
 * are manually deallocated, the cleanup handler can be popped off the stack.
 *
 * When an actual fault occurs, the MPI implementation will pop clenup
 * handlers off the stack and execute them in LIFO order.  This allows an MPI
 * program to emulate stack unwinding when a fault occurs, just as an
 * exception handler would.  It also allows libraries to register their own
 * cleanup code separately from the main application, preserving
 * composability.
 *
 * Each cleanup handler needs to return an MPI_Cleanup_code when it is done.
 * If any cleanup handler returns MPI_CLEANUP_ABORT, then the fault is
 * unrecoverable, and the entire application aborts.
 *
 * When all cleanup handlers return MPI_CLEANUP_SUCCESS, the semantics of
 * "success" are up to the implementor.  For example, a numerical library
 * might completely reinitialize itself, or it may require the application to
 * do some work to clean up its state, depending on how it is written.  Users
 * of MPI libraries are responsible for knowing how they should be cleaned up.
 *
 * @param[in]    start_state  State process will start in if cleanup succeeds.
 * @param[inout] state        Optional user parameter to the cleanup handler.
 */
typedef MPI_Cleanup_code (*MPI_Cleanup_handler)(
  MPI_Start_state start_state, void *state);
#define MPI_CLEANUP_HANDLER_NULL ((MPI_Cleanup_handler*) 0)

/*!
 * Push a cleanup handler onto this process's stack of cleanup handlers.  The
 * handler will be executed in LIFO order when a fault occurs, assuming it's
 * not popped before then.
 */
int MPI_Cleanup_handler_push(const MPI_Cleanup_handler handler, void *state);

/*!
 * Pop the last registered handler off the cleanup handler stack.  If there
 * are no handlers remaining on the stack, then this call will write out
 * MPI_CLEANUP_HANDLER_NULL for the handler, and state will be set to NULL.
 *
 * @param[out] handler  Where to store the popped handler.
 * @param[out] state    State that was to be passed to the handler on invocation.
 */
int MPI_Cleanup_handler_pop(
  const MPI_Cleanup_handler *handler, void **state);


// ===========================================================================
// Control how fault notifications are received
// ===========================================================================

/**
 * Control when fault interrupts are received on the current process.  If
 * synchronous, faults are only received on entry to MPI routines.  If
 * asynchronous, faults can be received at any time.  Use synchronous mode to
 * mask interrupts , e.g. for malloc.
 */
typedef enum {
  MPI_SYNCHRONOUS_FAULTS,
  MPI_ASYNCHRONOUS_FAULTS
} MPI_Fault_mode;


/*!
 * Test for faults synchronously.  Can insert this into tight loops in
 * synchronous mode to avoid having one process run ahead.
 *
 * If a fault is detected, this triggers a fault interrupt and entry into the
 * fault handler.
 */
int MPI_Fault_probe();


/**
 * Get current mode for receiving fault interrupts.
 *
 * @param[out] mode current MPI fault mode.
 */
int MPI_Get_fault_mode(MPI_Fault_mode *mode);


/**
 * Set MPI to the provided mode for receiving fault interrupts.
 *
 * @param[in] mode to switch fault event receipt to.
 */
int MPI_Set_fault_mode(MPI_Fault_mode mode);
