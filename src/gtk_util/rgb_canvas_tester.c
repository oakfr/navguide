#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <gtk/gtk.h>

#include <common/draw.h>
#include <gtk_util/rgb_canvas.h>

static gboolean 
canvas_motion( GtkuRgbCanvas *canvas, GdkEventMotion *event, 
        gpointer data )
{
    assert( canvas->buf );

    int x = event->x;
    int y = event->y;
    int r = 10;

    draw_ellipse_rgb( canvas->buf, x-r, y-r, x+r, y+r, 0x8000ff00, 1, 0,
            canvas->buf_width, canvas->buf_height, canvas->buf_stride );

    gtk_widget_queue_draw_area( GTK_WIDGET(canvas), x-r, y-r, 2*r, 2*r );

    return TRUE;
}

int main(int argc, char **argv)
{
    printf("rgb_canvas test\n");

    GtkWidget *window;

    gtk_init( &argc, &argv );

    window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    g_signal_connect( G_OBJECT(window), "delete_event", gtk_main_quit, NULL );
    g_signal_connect( G_OBJECT(window), "destroy", gtk_main_quit, NULL );
    gtk_container_set_border_width( GTK_CONTAINER(window), 10 );

    GtkWidget *canvas;
    canvas = GTK_WIDGET(gtku_rgb_canvas_new());
    gtk_widget_set_size_request(canvas, 200, 200);

    gtk_widget_set_events(canvas, GDK_LEAVE_NOTIFY_MASK
            | GDK_BUTTON_PRESS_MASK
            | GDK_BUTTON_RELEASE_MASK
            | GDK_POINTER_MOTION_MASK );

    g_signal_connect( G_OBJECT(canvas), "motion_notify_event", 
           G_CALLBACK(canvas_motion), NULL );

    gtk_container_add( GTK_CONTAINER(window), canvas );

    gtk_widget_show_all( window );
    gtk_main();

    return 0;
}
