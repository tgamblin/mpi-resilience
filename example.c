#include <stdint.h>

#include <mpi.h>
#include "mpi-resilience.h"

// ===========================================================================
// Various pseudo-code routines for this example.
// ===========================================================================
extern int deallocate_app_data();
extern int reinit_libraries();
extern int initialize_libraries();

extern int store_checkpoint();
extern int can_load_checkpoint_from_memory();
extern int load_checkpoint_from_memory();
extern int load_checkpoint_from_filesystem();

extern int physics_looks_ridiculous();
extern int converged();

extern int parse_start_step();
extern int can_run_at_size();
extern int MAX_STEP;


// ===========================================================================
// Application cleanup handler.
// ===========================================================================
MPI_Cleanup_code application_cleanup_handler(MPI_Step step) {
  if (!deallocate_app_data()) {
    return MPI_CLEANUP_ABORT;
  }

  if (!reinit_libraries()) {
    return MPI_CLEANUP_ABORT;
  }

  return MPI_CLEANUP_SUCCESS;
}


// ===========================================================================
// Stupid physics routine with fault polling.
// ===========================================================================
void do_physics() {
  while (!converged()) {
    // do some work.
    MPI_Fault_probe();
  }
}


// ===========================================================================
// Real main method of the application.  This is the entry point for rollbacks.
// ===========================================================================
void resilient_main(int argc, char **argv, MPI_Step start_step) {
  // Check if the new world size is acceptable.  If it is not, then just abort.
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if (can_run_at_size(size)) {
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  // Load a checkpoint based on start_step.  If this is a restart, it will be
  // determined by consensus.  If it is a regular start, it's determined based
  // on what was passed to Reinit.
  if (can_load_checkpoint_from_memory(start_step)) {
    load_checkpoint_from_memory(start_step);
  } else {
    load_checkpoint_from_filesystem(start_step);
  }

  // Main restart loop for the application.  An iteration of this loop is the
  // granularity that we can restore a checkpoint from.
  for (MPI_Step i=start_step; i < MAX_STEP; i++) {
    // Real application work.
    do_physics();

    // Application's own check for faults (assuming it knows how)
    if (physics_looks_ridiculous()) {
      MPI_Fault();
    }

    // Checkpoint store routine.
    store_checkpoint(i);

    // Mark completion of this step so that MPI can recover to here.
    MPI_Step_complete(i);
  }
}


// ===========================================================================
// C main function.  Handles basic MPI stuff and sets up a resilient entry
// point by calling MPI_Reinit().
//
// MPI_Reinit is necessary so that there is some valid spot on the stack to use
// when we come back from a fault.  Also makes it easy to clean up the OLD stack
// with things like setjmp and longjmp.
// ===========================================================================
int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  // register the global app cleanup handler here.
  MPI_Cleanup_handler_push(application_cleanup_handler);

  // init libraries.  Libraries are free to regsiter their own cleanup handlers.
  initialize_libraries(MPI_COMM_WORLD);

  // Application looks at command line or environment to determine the start
  // step we're supposed to execute from.
  MPI_Step start_step = parse_start_step(argc, argv);

  // This is the point at which the resilient MPI program starts.  We pass the
  // default start step so that the first invocation starts there.
  MPI_Reinit(argc, argv, resilient_main, start_step);

  MPI_Finalize();
}
