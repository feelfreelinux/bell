#ifndef BELL_TASK_H
#define BELL_TASK_H

#ifdef ESP_PLATFORM
#include <esp_pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#include <pthread.h>

namespace bell
{
    class Task
    {
    public:
        std::string taskName; 
        int stackSize, core;
        Task(std::string taskName, int stackSize, int core) {
            this->taskName = taskName;
            this->stackSize = stackSize;
            this->core = core;
        }
        virtual ~Task() {}

        bool startTask()
        {
#ifdef ESP_PLATFORM
            esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
            cfg.stack_size = stackSize;
            cfg.inherit_cfg = true;
            cfg.thread_name = this->taskName.c_str();
            cfg.pin_to_core = core;
            esp_pthread_set_cfg(&cfg);
#endif
            return (pthread_create(&_thread, NULL, taskEntryFunc, this) == 0);
        }
        void waitForTaskToReturn()
        {
            (void)pthread_join(_thread, NULL);
        }

    protected:
        virtual void runTask() = 0;

    private:
        static void *taskEntryFunc(void *This)
        {
            ((Task *)This)->runTask();
            return NULL;
        }

        pthread_t _thread;
    };
}

#endif