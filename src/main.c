#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <libnotify/notify.h>

#define EXIT_ERR_INVALID_ARGUMENT 1
#define EXIT_ERR_INVALID_FILE_PATH 2

const char *program = "watchfd";

int main(int argc, char *argv[]) {
  char *base_path = NULL;
  char *token = NULL;

  if (argc < 2) {
    fprintf(stderr, "USAGE: %s PATH\n", argv[0]);
    return EXIT_ERR_INVALID_ARGUMENT;
  }

  base_path = (char *)malloc(sizeof(char) * (strlen(argv[1]) + 1));
  strcpy(base_path, argv[1]);

  token = strtok(base_path, "/");
  while (token != NULL) {
    base_path = token;
    token = strtok(NULL, "/");
  }

  if (base_path == NULL) {
    fprintf(stderr, "Invalid file path: %s", argv[1]);
    return EXIT_ERR_INVALID_FILE_PATH;
  }

  bool notify_status = notify_init(program);

  if (!notify_status) {
    fprintf(stderr, "Failed to initialize libnotify");
    return EXIT_FAILURE;
  }

  int ino_fd = inotify_init();

  if (ino_fd == -1) {
    fprintf(stderr, "Error initializing inotify instance\n");
    return EXIT_FAILURE;
  }

  const uint32_t ino_watch_msk = IN_CREATE | IN_DELETE | IN_ACCESS |
                                 IN_CLOSE_WRITE | IN_MODIFY | IN_MOVE_SELF;

  int ino_watch = inotify_add_watch(ino_fd, argv[1], ino_watch_msk);

  if (ino_watch == -1) {
    fprintf(stderr, "Failed to watch file at %s. Error: %s", argv[1],
            strerror(errno));

    return EXIT_FAILURE;
  }

  const struct inotify_event *watch_event;
  char buf[4096];
  ssize_t buf_size;
  NotifyNotification *notifier;
  char *notification_msg = NULL;

  while (true) {
    printf("Watching file for events...\n");

    buf_size = read(ino_fd, buf, sizeof(buf));

    if (buf_size == -1) {
      fprintf(stderr, "Failed to read from inotify instance\n");
      return EXIT_FAILURE;
    }

    for (char *ptr = buf; ptr < buf + buf_size;
         ptr += sizeof(struct inotify_event) + watch_event->len) {

      watch_event = (const struct inotify_event *)ptr;

      notification_msg = NULL;

      if (watch_event->mask & IN_CREATE)
        notification_msg = "File created\n";
      if (watch_event->mask & IN_DELETE)
        notification_msg = "File deleted\n";
      if (watch_event->mask & IN_ACCESS)
        notification_msg = "File accessed\n";
      if (watch_event->mask & IN_CLOSE_WRITE)
        notification_msg = "File closed after being written to\n";
      if (watch_event->mask & IN_MODIFY)
        notification_msg = "File modified\n";
      if (watch_event->mask & IN_MOVE_SELF)
        notification_msg = "File moved\n";

      if (notification_msg == NULL)
        continue;

      notifier = notify_notification_new(base_path, notification_msg,
                                         "dialog-information");

      if (notifier == NULL) {
        fprintf(stderr, "Failed to create new notification\n");
        continue;
      }

      if (!notify_notification_show(notifier, NULL)) {
        fprintf(stderr, "Failed to show notification\n");
      }

      g_object_unref(G_OBJECT(notifier));
    }
  }

  close(ino_fd);
  notify_uninit();
  free(base_path);

  return EXIT_SUCCESS;
}
