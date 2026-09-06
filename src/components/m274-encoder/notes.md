main.c

Now you can control initialization order:

#include "m274-encoder.h"

void app_main(void)
{
    // Create encoder resources
    ESP_ERROR_CHECK(m274_encoder_init());

    // Initialize other components...
    // gui_init();
    // motor_init();
    // etc.

    // Start encoder hardware
    ESP_ERROR_CHECK(m274_encoder_start());
}

And later, when you have the GUI:

static void encoder_changed(int32_t value)
{
    gui_set_value(value);
}

void app_main(void)
{
    ESP_ERROR_CHECK(m274_encoder_init());

    m274_encoder_set_callback(encoder_changed);

    // Other initialization...

    ESP_ERROR_CHECK(m274_encoder_start());
}

That gives you a nice separation:

m274_encoder_init()
        ↓
create software resources
        ↓
other components initialize
        ↓
m274_encoder_start()
        ↓
encoder hardware + task running
        ↓
encoder event
        ↓
callback
        ↓
GUI

One thing to consider later: don't call heavy GUI operations
directly from encoder_task. For a GUI like LVGL, it's usually better for
the callback to post an event/message to the GUI task.

Yes. The version I gave you **does have the queue**.

 The flow is:

```
esp-idf-lib callback
        ↓
  event_queue
        ↓
  encoder_task()
        ↓
 encoder_callback()
```

 Specifically, these are the queue parts:

```
static QueueHandle_t event_queue = NULL;
```

 Created in `m274_encoder_init()`:

```
event_queue = xQueueCreate(
    EV_QUEUE_LEN,
    sizeof(rotary_encoder_event_t)
);
```

 Library callback puts events into it:

```
xQueueSendToBack(queue, event, 0);
```

 And `encoder_task()` consumes them:

```
xQueueReceive(event_queue, &event, portMAX_DELAY);
```

 Then for `RE_ET_CHANGED`:

```
encoder_value += event.diff;

if (encoder_callback != NULL) {
    encoder_callback(encoder_value);
}
```

 So **yes, the queue is fully implemented in the component**. You don't need to create another queue in `main`.
