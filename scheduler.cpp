#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include "scheduler.h"

#include <assert.h>
#include <curses.h>
#include <ucontext.h>
#include <string.h>

#include "util.h"

// This is an upper limit on the number of tasks we can create.
#define MAX_TASKS 128

// This is the size of each task's stack memory
#define STACK_SIZE 65536

// This struct will hold the all the necessary information for each task
typedef struct task_info {
  // This field stores all the state required to switch back to this task
  ucontext_t context;

  // This field stores another context. This one is only used when the task
  // is exiting.
  ucontext_t exit_context;

  //   a. Keep track of this task's state.
  bool asleep;

  //   b. If the task is sleeping, when should it wake up?
  time_t wakeup;

  //   c. If the task is waiting for another task, which task is it waiting for?
  task_t waiting_task;

  //   d. Was the task blocked waiting for user input? Once you successfully
  //      read input save it here so it can be returned.
  int input;

  // Keep track if the task has ended
  bool finished;
} task_info_t;

int current_task = 0;          //< The handle of the currently-executing task
int num_tasks = 1;             //< The number of tasks created so far
task_info_t tasks[MAX_TASKS];  //< Information for every task


/**
 * Find the next ready task
 * \param Current task
 */
task_t findReadyTask(task_t curTask){
  // Start at the current task and search from there
  task_t nextTask = curTask;

  // We'll loop until we've found a suitable task.
  while(true){
    nextTask = (nextTask + 1) % num_tasks;

    // If any are ready for some reason, then prob just go with them
    if(tasks[nextTask].asleep == false){
      return nextTask;
    }

    // Otherwise, check if one has a time that it's waiting for
    if(tasks[nextTask].wakeup != -1){
      if(tasks[nextTask].wakeup <= time_ms()){
        return nextTask;
      }
    }

    // If not, check if it's waiting for another task
    else if(tasks[nextTask].waiting_task != -1){
      if(tasks[tasks[nextTask].waiting_task].finished){
        return nextTask;
      }
    }

    // Finally, check if it's waiting for input
    else if(tasks[nextTask].input == '\0'){
      int temp = getch(); // If it was waiting for input, see if we have input
      if(temp != ERR){
        tasks[nextTask].input = temp; // Since we do, we'll store it incase the keypress isn't happening at the next time
        return nextTask;
      }
    }
  }
}



/**
 * This function will execute when a task's function returns. This allows you
 * to update scheduler states and start another task. This function is run
 * because of how the contexts are set up in the task_create function.
 */
void task_exit() {
  // We want to set all of the indicators to show that the task isn't waiting
  // This means it will never be called after it's finished
  tasks[current_task].wakeup = -1;
  tasks[current_task].waiting_task = -1;
  tasks[current_task].input = '\1';
  tasks[current_task].asleep = true;

  // We also want to indicate that it's finished so our task_wait can check
  tasks[current_task].finished = true;

  // Then we do our normal context swap
  task_t nextTask = findReadyTask(current_task);
  int prevTask = current_task;  // Storing the current_task
  current_task = nextTask;      // and updating it for the next one
  swapcontext(&tasks[prevTask].context, &tasks[nextTask].context); // Swap
}

/**
 * Create a new task and add it to the scheduler.
 *
 * \param handle  The handle for this task will be written to this location.
 * \param fn      The new task will run this function.
 */
void task_create(task_t* handle, task_fn_t fn) {
  // Claim an index for the new task
  int index = num_tasks;
  num_tasks++;

  // Set the task handle to this index, since task_t is just an int
  *handle = index;

  // We're going to make two contexts: one to run the task, and one that runs at the end of the task
  // so we can clean up. Start with the second

  // First, duplicate the current context as a starting point
  getcontext(&tasks[index].exit_context);

  // Set up a stack for the exit context
  tasks[index].exit_context.uc_stack.ss_sp = malloc(STACK_SIZE);
  tasks[index].exit_context.uc_stack.ss_size = STACK_SIZE;

  // Set up a context to run when the task function returns. This should call task_exit.
  makecontext(&tasks[index].exit_context, task_exit, 0);

  // Now we start with the task's actual running context
  getcontext(&tasks[index].context);

  // Allocate a stack for the new task and add it to the context
  tasks[index].context.uc_stack.ss_sp = malloc(STACK_SIZE);
  tasks[index].context.uc_stack.ss_size = STACK_SIZE;

  // Now set the uc_link field, which sets things up so our task will go to the exit context when
  // the task function finishes
  tasks[index].context.uc_link = &tasks[index].exit_context;

  // Set all of the indicators to their defaults
  tasks[index].asleep = false;
  tasks[index].wakeup = -1;
  tasks[index].waiting_task = -1;
  tasks[index].input = '\1';
  tasks[index].finished = false;

  // And finally, set up the context to execute the task function
  makecontext(&tasks[index].context, fn, 0);
}

/**
 * Wait for a task to finish. If the task has not yet finished, the scheduler should
 * suspend this task and wake it up later when the task specified by handle has exited.
 *
 * \param handle  This is the handle produced by task_create
 */
void task_wait(task_t handle) {
  // Now we block the task, and set the indicator var (waiting_task) to the task it should wait for
  tasks[current_task].asleep = true;
  tasks[current_task].waiting_task = handle;

  // We also have to make sure that the other indicators are set to their un-blocked states
  tasks[current_task].wakeup = -1;
  tasks[current_task].input = '\1';


  // Context swap
  task_t nextTask = findReadyTask(current_task); // Find the next un-blocked task
  int prevTask = current_task;  // Store the current_task
  current_task = nextTask;      // and update it for the next one
  swapcontext(&tasks[prevTask].context, &tasks[nextTask].context); // Swap

  // When we get back into this task, it's been unblocked
  tasks[prevTask].asleep = false;
}

/**
 * The currently-executing task should sleep for a specified time. If that time is larger
 * than zero, the scheduler should suspend this task and run a different task until at least
 * ms milliseconds have elapsed.
 *
 * \param ms  The number of milliseconds the task should sleep.
 */
void task_sleep(size_t ms) {
  // Now we block the task, and set the indicator var (wakeup) to our wakeup time
  tasks[current_task].wakeup = time_ms()+ms;
  tasks[current_task].asleep = true;

  // We also have to make sure that the other indicators are set to their un-blocked states
  tasks[current_task].waiting_task = -1;
  tasks[current_task].input = '\1';


  // Context swap
  task_t nextTask = findReadyTask(current_task); // Find the next un-blocked task
  int prevTask = current_task;  // Store the current_task
  current_task = nextTask;      // and update it for the next one
  swapcontext(&tasks[prevTask].context, &tasks[nextTask].context); // Swap

  // When we get back into this task, it's been unblocked
  tasks[prevTask].asleep = false;
}

/**
 * Read a character from user input. If no input is available, the task should
 * block until input becomes available. The scheduler should run a different
 * task while this task is blocked.
 *
 * \returns The read character code
 */
int task_readchar() {
  // To check for input, call getch(). If it returns ERR, no input was available.
  // Otherwise, getch() will returns the character code that was read.
  int temp = getch();   // get character input
  if (temp == ERR){     // if no input is available

    // Now we block the task, and set the indicator var (input) to our waiting value
    tasks[current_task].input = '\0';
    tasks[current_task].asleep = true;

    // We also have to make sure the other indicators are set to their un-blocked states
    tasks[current_task].wakeup = -1;
    tasks[current_task].waiting_task = -1;

    // Context swap
    task_t nextTask = findReadyTask(current_task); // Find the next un-blocked task
    int prevTask = current_task;  // Store the current_task
    current_task = nextTask;      // and updating it for the next one
    swapcontext(&tasks[prevTask].context, &tasks[nextTask].context); // Swap

    // Once we get back to this task, we must have gotten an input
    // and we would have stored it, so we'll just return that
    return tasks[prevTask].input;
  }
  return temp;
}
