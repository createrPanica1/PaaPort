#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

const std::vector<int> common_ports = {
    20, 21, 22, 23, 25, 26, 53, 80, 110, 115, 135,
    139, 143, 443, 445, 587, 993, 995, 1723, 3306,
    3389, 5900, 8080
};

GtkWidget *window, *ip_entry, *results_text, *scan_button, *progress_bar, *time_label;
bool scanning = false;

const gchar *cyber_style = R"(
    * {
        background-color: #000000;
        color: #00FF00;
        font-family: 'Terminus', Monospace;
        font-size: 14px;
    }
    
    GtkWindow {
        background-image: linear-gradient(to bottom right, #001100, #000000);
        border: 2px solid #00FF00;
    }
    
    GtkEntry {
        background-color: #001100;
        border: 1px solid #00FF00;
        box-shadow: 0 0 5px #00FF00;
        padding: 5px;
    }
    
    GtkButton {
        background-image: linear-gradient(to bottom, #002200, #001100);
        border: 1px solid #00FF00;
        border-radius: 3px;
        box-shadow: 0 0 8px #00FF00;
        padding: 8px 20px;
    }
    
    GtkButton:hover {
        background-image: linear-gradient(to bottom, #004400, #002200);
        box-shadow: 0 0 12px #00FF00;
    }
    
    GtkProgressBar {
        background-color: #001100;
        border: 1px solid #00FF00;
        border-radius: 3px;
    }
    
    GtkProgressBar trough {
        background-color: #000000;
        border: 1px solid #00FF00;
    }
    
    GtkProgressBar progress {
        background-color: #00FF00;
        box-shadow: 0 0 5px #00FF00;
    }
    
    GtkTextView {
        background-color: #000000;
        border: 1px solid #00FF00;
        caret-color: #00FF00;
    }
    
    GtkScrolledWindow {
        background-color: #000000;
        border: 1px solid #00FF00;
    }
    
    .cyber-label {
        text-shadow: 0 0 5px #00FF00;
    }
)";

bool is_port_open(const std::string &ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    connect(sock, (sockaddr*)&addr, sizeof(addr));

    fd_set set;
    FD_ZERO(&set);
    FD_SET(sock, &set);

    timeval timeout{1, 0};
    bool open = (select(sock+1, NULL, &set, NULL, &timeout) > 0);
    close(sock);
    return open;
}

void reset_interface() {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(results_text));
    gtk_text_buffer_set_text(buffer, "", -1);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    gtk_label_set_text(GTK_LABEL(time_label), "");
    gtk_entry_set_text(GTK_ENTRY(ip_entry), "");
}

void update_gui(std::string ip) {
    scanning = true;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(results_text));
    gtk_text_buffer_set_text(buffer, "=== SCANNING STARTED ===\n", -1);
    
    auto start = std::chrono::steady_clock::now();
    int open_count = 0;

    for (size_t i = 0; i < common_ports.size(); ++i) {
        int port = common_ports[i];
        
        if (is_port_open(ip, port)) {
            open_count++;
            GtkTextIter iter;
            gtk_text_buffer_get_end_iter(buffer, &iter);
            gtk_text_buffer_insert_markup(buffer, &iter,
                ("<span foreground=\"#00FF00\">• PORT " + 
                std::to_string(port) + 
                " [OPEN]</span>\n").c_str(), -1);
        }

        double progress = static_cast<double>(i+1) / common_ports.size();
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_bar));
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), progress);

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        gtk_label_set_markup(GTK_LABEL(time_label),
            ("<span foreground=\"#00FF00\">⏱ TIME: " + 
            std::to_string(elapsed) + 
            "s</span>").c_str());

        while (gtk_events_pending())
            gtk_main_iteration_do(FALSE);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert_markup(buffer, &iter,
        ("\n<span foreground=\"#00FF00\" weight=\"bold\">✓ SCAN COMPLETE\n"
        "OPEN PORTS: " + 
        std::to_string(open_count) + 
        "</span>\n").c_str(), -1);
    
    scanning = false;
    gtk_widget_set_sensitive(scan_button, TRUE);
    gtk_button_set_label(GTK_BUTTON(scan_button), "SCAN PORTS");
}

void on_scan_clicked(GtkWidget *widget, gpointer data) {
    if(scanning) return;
    
    const char *ip = gtk_entry_get_text(GTK_ENTRY(ip_entry));
    if (strlen(ip) == 0) return;

    reset_interface();
    gtk_widget_set_sensitive(scan_button, FALSE);
    gtk_button_set_label(GTK_BUTTON(scan_button), "SCANNING...");
    std::thread(update_gui, std::string(ip)).detach();
}

void build_gui() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "PaPort n0.0.1");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    ip_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ip_entry), "ENTER TARGET IP");
    gtk_widget_set_margin_bottom(ip_entry, 10);
    gtk_box_pack_start(GTK_BOX(vbox), ip_entry, FALSE, FALSE, 0);

    scan_button = gtk_button_new_with_label("SCAN PORTS");
    g_signal_connect(scan_button, "clicked", G_CALLBACK(on_scan_clicked), NULL);
    gtk_widget_set_margin_bottom(scan_button, 10);
    gtk_box_pack_start(GTK_BOX(vbox), scan_button, FALSE, FALSE, 0);

    progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 0);

    time_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(time_label), 0);
    gtk_box_pack_start(GTK_BOX(vbox), time_label, FALSE, FALSE, 0);

    results_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(results_text), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(results_text), TRUE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(results_text), FALSE);
    
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), results_text);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, cyber_style, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_widget_show_all(window);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    build_gui();
    gtk_main();
    return 0;
}
