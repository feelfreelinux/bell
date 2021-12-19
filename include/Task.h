#ifndef BELL_TASK_H
#define BELL_TASK_H

#ifdef ESP_PLATFORM
#include <esp_pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
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
        bool isRunning = false;
        bool runOnPSRAM;
        Task(std::string taskName, int stackSize, int priority, int core, bool runOnPSRAM = true)
        {
            this->taskName = taskName;
            this->stackSize = stackSize;
            this->core = core;
            this->runOnPSRAM = runOnPSRAM;
#ifdef ESP_PLATFORM
			this->priority = ESP_TASK_PRIO_MIN + 1 + priority;
			if (this->priority < 0) this->priority = ESP_TASK_PRIO_MIN + 1;
#endif
        }
        virtual ~Task() {}

        bool startTask()
        {
#ifdef ESP_PLATFORM
            if (runOnPSRAM)
            {

                xTaskBuffer = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                xStack = (StackType_t *)heap_caps_malloc(this->stackSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

                return (xTaskCreateStaticPinnedToCore(taskEntryFuncPSRAM, this->taskName.c_str(), this->stackSize, this, 2, xStack, xTaskBuffer, this->core) != NULL);
            }
            else
            {
                esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
                cfg.stack_size = stackSize;
                cfg.inherit_cfg = true;
                cfg.thread_name = this->taskName.c_str();
                cfg.pin_to_core = core;
				cfg.prio = priority;
                esp_pthread_set_cfg(&cfg);
            }
#endif
            return (pthread_create(&_thread, NULL, taskEntryFunc, this) == 0);
        }
        void waitForTaskToReturn()
        {
#ifdef ESP_PLATFORM
            if (runOnPSRAM)
            {
                while (isRunning)
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
                heap_caps_free(xStack);
				// TCB are cleanup in IDLE task, so give it some time
				TimerHandle_t timer = xTimerCreate( "cleanup", pdMS_TO_TICKS(5000), pdFALSE, xTaskBuffer, 
													[](TimerHandle_t xTimer) {
														heap_caps_free(pvTimerGetTimerID(xTimer));
														xTimerDelete(xTimer, portMAX_DELAY);
													} );	
				xTimerStart(timer, portMAX_DELAY);
            }
            else
            {
                (void)pthread_join(_thread, NULL);
            }
#else
            (void)pthread_join(_thread, NULL);
#endif
        }

    protected:
        virtual void runTask() = 0;

    private:
#ifdef ESP_PLATFORM
		int priority;
        StaticTask_t *xTaskBuffer;
        StackType_t *xStack;
#endif
        static void *taskEntryFunc(void *This)
        {
            ((Task *)This)->runTask();
            return NULL;
        }
#ifdef ESP_PLATFORM
        static void taskEntryFuncPSRAM(void *This)
        {

            ((Task *)This)->isRunning = true;

            Task *self = (Task *)This;
            self->isRunning = true;
            self->runTask();
            self->isRunning = false;
            vTaskDelete(NULL);
        }
#endif

        pthread_t _thread;
    };
}

#endif