#include <stdint.h>
#include <limits.h>

#include <mpi.h>
#include "mpi-resilience.h"

// ===========================================================================
// Global variable for the step we should restart at.
// ===========================================================================
static int time_step;


// ===========================================================================
// Various pseudo-code routines for this example.
// ===========================================================================
extern int deallocate_app_data();
extern int reinit_libraries();
extern int initialize_libraries();

extern int store_checkpoint();
extern int can_load_checkpoint_from_memory();
extern int last_checkpoint_on_disk(int rank);
extern int load_checkpoint_from_memory();
extern int load_checkpoint_from_filesystem();

extern int have_neighbor_checkpoint_for(int rank);
extern int send_neighbor_checkpoint_to(int rank);
extern int receive_neighbor_checkpoint();

extern int converged();
extern int physics_looks_ridiculous();
extern int parse_start_step(int argc, char **argv);
extern int can_run_at_size();
extern int MAX_STEP;


// ===========================================================================
// Application cleanup handler.
// ===========================================================================
MPI_Cleanup_code application_cleanup_handler(MPI_Start_state start_state,
                                             void *state)
{
  if (!deallocate_app_data()) {
    return MPI_CLEANUP_ABORT;
  }

  if (!reinit_libraries()) {
    return MPI_CLEANUP_ABORT;
  }

  return MPI_CLEANUP_SUCCESS;
}


// ===========================================================================
// Real main method of the application.  This is the entry point for rollbacks.
// ===========================================================================
void resilient_main(int argc, char **argv, MPI_Start_state start_state) {
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // Check if the new world size is acceptable.  If it is not, then just abort.
  if (can_run_at_size(size)) {
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  if (start_state != MPI_START_NEW) {
    // Figure out who died.
    int i_died = (start_state == MPI_START_ADDED ? 1 : 0);
    int someone_died, who_died;
    MPI_Allreduce(&i_died, &someone_died, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD);
    MPI_Allreduce(&i_died, &who_died, 1, MPI_INT, MPI_MAXLOC, MPI_COMM_WORLD);

    if (someone_died) {
      int have_cp = have_neighbor_checkpoint_for(who_died);
      MPI_Allreduce(MPI_IN_PLACE, &have_cp, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD);

      if (i_died) {
        if (have_cp) {
          time_step = receive_neighbor_checkpoint();
        } else {
          time_step = last_checkpoint_on_disk(rank);
        }
      } else if (have_neighbor_checkpoint_for(who_died)) {
        send_neighbor_checkpoint_to(who_died);
      }
    }

    // Take the minimum reached time step and start from there.
    MPI_Allreduce(MPI_IN_PLACE, &time_step, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
  }

  // Load a checkpoint based on start_step.  If this is a restart, it will be
  // determined by consensus.  If it is a regular start, it's determined based
  // on what was passed to Reinit.
  if (can_load_checkpoint_from_memory(time_step)) {
    load_checkpoint_from_memory(time_step);
  } else {
    load_checkpoint_from_filesystem(time_step);
  }

  //
  // Main restart loop for the application.
  //
  for (; time_step < MAX_STEP; time_step++) {
    // Real application work.
    while (!converged()) {
      // ... do some work ...
      MPI_Fault_probe();
    }

    // Application's own check for faults (assuming it knows how)
    if (physics_looks_ridiculous()) {
      MPI_Fault();
    }

    // Checkpoint store routine.
    store_checkpoint(time_step);
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
  MPI_Cleanup_handler_push(application_cleanup_handler, 0);

  // init libraries.  Libraries are free to regsiter their own cleanup handlers.
  initialize_libraries(MPI_COMM_WORLD);

  // Set up time step based on command line parmeters.
  time_step = parse_start_step(argc, argv);

  // This is the point at which the resilient MPI program starts.  We pass the
  // default start step so that the first invocation starts there.
  MPI_Reinit(argc, argv, resilient_main);

  MPI_Finalize();
}
