#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <alsa/asoundlib.h>

//ALSA wrappers
snd_seq_event_t ev;

void send_note(snd_seq_t* seq, int port, int chn, int note, bool noteon, bool loud) {
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    if (noteon) {
        snd_seq_ev_set_noteon(&ev, chn, 51 + note, 100);
        if (loud) {printf("NoteOn %d, Chn %d\n", note, chn);};
    } else {
        snd_seq_ev_set_noteoff(&ev, chn, 51 + note, 0);
        if (loud) {printf("NoteOff %d, Chn %d\n", note, chn);};
    }
    snd_seq_event_output(seq, &ev);
    snd_seq_drain_output(seq);
}


class MidiEventRecorder {
public:
    MidiEventRecorder(snd_seq_t* seq, int port) : seq(seq), port(port), is_recording(false), is_looping(false)  {}

    void startRecording() {
        is_recording = true;
        recorded_events.clear();
    }

    void stopRecording() {
        is_recording = false;
    }

    void sendNote(int chn, int note, bool noteon, bool loud) {
        if (is_recording) {
            recordEvent(chn, note, noteon, loud);
        }

        sendNoteEvent(chn, note, noteon, loud);
    }

    void loopRecordedEventsNonBlocking() {
        is_looping=true;
        std::thread([this] {
            while (is_looping) {
                if (recorded_events.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Adjust as needed
                    continue;
                }

                for (const auto& recorded_event : recorded_events) {
                    sendNoteEvent(recorded_event.channel, recorded_event.note, recorded_event.noteon, false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Adjust as needed
                }
            }
        }).join();
    }
    
    bool IsEmpty() const {
        return recorded_events.empty();
    }
    
    bool isLooping() const {
        return is_looping;
    }
    
    void stop_looping() {
    is_looping=false;
    }
    bool isRecording() const {
        return is_recording;
    }
    
private:
    struct RecordedEvent {
        int channel;
        int note;
        bool noteon;
    };

    snd_seq_t* seq;
    int port;
    bool is_recording;
    bool is_looping;
    std::vector<RecordedEvent> recorded_events;

    void recordEvent(int chn, int note, bool noteon, bool loud) {
        recorded_events.push_back({chn, note, noteon});
    }

    void sendNoteEvent(int chn, int note, bool noteon, bool loud) {
        snd_seq_ev_clear(&ev);
        snd_seq_ev_set_source(&ev, port);
        snd_seq_ev_set_subs(&ev);
        snd_seq_ev_set_direct(&ev);

        if (noteon) {
            snd_seq_ev_set_noteon(&ev, chn, 51 + note, 100);
            if (loud){
            printf("NoteOn %d, Chn %d\n", note, chn);}
        } else {
            snd_seq_ev_set_noteoff(&ev, chn, 51 + note, 0);
            if (loud){
            printf("NoteOff %d, Chn %d\n", note, chn);}
        }

        snd_seq_event_output(seq, &ev);
        snd_seq_drain_output(seq);
    }
};


//EVENT NUMBER FINDER
std::string findEventDevice(const std::string& deviceName) {
    std::ifstream devicesFile("/proc/bus/input/devices");
    std::string line;
    std::string foundDevice;

    while (std::getline(devicesFile, line)) {
        if (line.find(deviceName) != std::string::npos) {
            // Look for the line containing "event"
            while (std::getline(devicesFile, line)) {
                if (line.find("event") != std::string::npos) {
                    // Extract the event number
                    size_t pos = line.find("event");
                    foundDevice = "/dev/input/" + line.substr(pos, line.find(" ", pos) - pos);
                    break;
                }
            }
            break;
        }
    }

    return foundDevice;
}



//GLOBALS
bool islooping = false;
int crr_chn = 0;
std::vector<MidiEventRecorder> midiRecorders;


//EVENT HANDLERS
//note events
void drum(int k, snd_seq_t* seq, int port, int q) {
    if (q == 0) {
        send_note(seq, port, 0, k, false, true);
    } else {
        send_note(seq, port, 0, k, true, true);
    }
}

void note(int k, snd_seq_t* seq, int port, int q) {
    if (q == 0) {
        // Send note events only to the currently selected deck
        if (crr_chn >= 0 && crr_chn < midiRecorders.size()) {
            midiRecorders[crr_chn].sendNote(crr_chn + 1, k, false, true);
            printf("%c", crr_chn);
        } else {
            printf("Invalid deck index: %d\n", crr_chn);
        }
        
    } else {
        // Send note events only to the currently selected deck
        if (crr_chn >= 0 && crr_chn < midiRecorders.size()) {
            midiRecorders[crr_chn].sendNote(crr_chn + 1, k, true, true);
            printf("%c", crr_chn);
        } else {
            printf("Invalid deck index: %d\n", crr_chn);
        }
    }
}

// Decks
void deck(int k, snd_seq_t* seq, int port, int q) {
    if (q != 0) {
        if (k >= 0 && k < midiRecorders.size()) {
            MidiEventRecorder& recorder = midiRecorders[k];
            if (recorder.IsEmpty()) {
                crr_chn = k;
            } else if (recorder.isLooping()) {
                recorder.stopRecording();
                recorder.loopRecordedEventsNonBlocking();
                printf("Activating deck %d\n", (k + 1));
            } else {
            	recorder.stop_looping();
                recorder.loopRecordedEventsNonBlocking();
                printf("Mute deck %d\n", (k + 1));
            }
        } else {
            // Handle the case where k is an invalid index
            printf("Invalid deck index: %d\n", k);
        }
    }
}

// Controllers
void loop(int k, snd_seq_t* seq, int port, int q) {
    printf("WAth the Helll, Oh my goood");
    if (q != 0) {
        if (crr_chn >= 0 && crr_chn < midiRecorders.size()) {
            MidiEventRecorder& recorder = midiRecorders[crr_chn];

            if (recorder.isRecording()) {
                printf("Replay\n");
                recorder.stopRecording();
                recorder.loopRecordedEventsNonBlocking();
            } else {
            	printf("Record\n");
                recorder.startRecording();
            }
        } else {
            // Handle the case where crr_chn is an invalid index
            printf("Invalid deck index: %d\n", crr_chn);
        }
    }
}

void sequencer(int k, snd_seq_t* seq, int port, int q) {
    printf("Start/End seq");
}

void dropselect(int k, snd_seq_t* seq, int port, int q) {
    printf("Start selecting decks");
}

void drop(int k, snd_seq_t* seq, int port, int q) {
    printf("Activate drop decks");
}



//MAPPER
void handle_event(int k, snd_seq_t* seq, int port, char q) {
    printf("%c", q);
    switch (k) {
    case 1:
        printf("Closing...");
        for (auto& midiRecorder : midiRecorders){
             midiRecorder.stop_looping();};
        for (int chn = 0; chn < 16; ++chn) {
          for (int note = 0; note < 128; ++note) {
            send_note(seq, port, chn, note, false, false);}};
    
        snd_seq_close(seq);
        exit(0);
        break;

    case 82:
        sequencer(0, seq, port, q);
        break;

    case 28:
        loop(0, seq, port, q);
        break;

    case 99:
        deck(0, seq, port, q);
        break;
    case 70:
        deck(1, seq, port, q);
        break;
    case 119:
        deck(2, seq, port, q);
        break;
    case 110:
        deck(3, seq, port, q);
        break;
    case 102:
        deck(4, seq, port, q);
        break;
    case 104:
        deck(5, seq, port, q);
        break;
    case 111:
        deck(6, seq, port, q);
        break;
    case 107:
        deck(7, seq, port, q);
        break;
    case 109:
        deck(8, seq, port, q);
        break;

    case 98:
        drum(0, seq, port, q);
        break;
    case 71:
        drum(1, seq, port, q);
        break;
    case 72:
        drum(2, seq, port, q);
        break;
    case 73:
        drum(3, seq, port, q);
        break;
    case 75:
        drum(4, seq, port, q);
        break;
    case 76:
        drum(5, seq, port, q);
        break;
    case 77:
        drum(6, seq, port, q);
        break;
    case 79:
        drum(7, seq, port, q);
        break;
    case 80:
        drum(8, seq, port, q);
        break;
    case 81:
        drum(9, seq, port, q);
        break;

    case 2:
        note(0, seq, port, q);
        break;
    case 16:
        note(1, seq, port, q);
        break;
    case 17:
        note(2, seq, port, q);
        break;
    case 4:
        note(3, seq, port, q);
        break;
    case 18:
        note(4, seq, port, q);
        break;
    case 5:
        note(5, seq, port, q);
        break;
    case 19:
        note(6, seq, port, q);
        break;
    case 6:
        note(7, seq, port, q);
        break;
    case 20:
        note(8, seq, port, q);
        break;
    case 21:
        note(9, seq, port, q);
        break;
    case 8:
        note(10, seq, port, q);
        break;
    case 22:
        note(11, seq, port, q);
        break;
    case 9:
        note(12, seq, port, q);
        break;
    case 23:
        note(13, seq, port, q);
        break;
    case 24:
        note(14, seq, port, q);
        break;
    case 11:
        note(15, seq, port, q);
        break;
    case 25:
        note(16, seq, port, q);
        break;
    case 12:
        note(17, seq, port, q);
        break;
    case 26:
        note(18, seq, port, q);
        break;
    case 13:
        note(19, seq, port, q);
        break;
    case 27:
        note(20, seq, port, q);
        break;

    case 103:
        dropselect(0, seq, port, q);
        break;

    case 108:
        drop(0, seq, port, q);
        break;
    }
}



int main(int argc, char* argv[]) {
    //Open MIDI port
    snd_seq_t* seq;
    int client, port;
    snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0);
    client = snd_seq_client_id(seq);
    snd_seq_set_client_name(seq, "Nirvana707");
    port = snd_seq_create_simple_port(seq, "Out", SND_SEQ_PORT_TYPE_MIDI_GENERIC, SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ);
    snd_seq_connect_to(seq, port, 14, 0);
    //Initialize deck recorders
    for (int i = 0; i < 9; ++i) {
        MidiEventRecorder midiRecorder(seq, port);
        midiRecorders.push_back(midiRecorder);
    }
    
    //Grab keyboard and pass to the mapper
    struct input_event ev[64];
    int fevdev = -1;
    int result = 0;
    int size = sizeof(struct input_event);
    int rd;
    int value;
    char name[256] = "Unknown";
    std::string deviceName = "CHICONY USB Keyboard";
    std::string devicePath = findEventDevice(deviceName);

    // Check if the device was found
    if (devicePath.empty()) {
        std::cerr << "Device not found: " << deviceName << std::endl;
        exit(EXIT_FAILURE);
    }

    const char* device = devicePath.c_str();

    fevdev = open(device, O_RDONLY);
    if (fevdev == -1) {
        printf("Failed to open event device.\n");
        exit(1);
    }

    result = ioctl(fevdev, EVIOCGNAME(sizeof(name)), name);
    printf("Reading From : %s (%s)\n", device, name);

    printf("Getting exclusive access: ");
    result = ioctl(fevdev, EVIOCGRAB, 1);
    printf("%s\n", (result == 0) ? "SUCCESS" : "FAILURE");

    while (1) {
        if ((rd = read(fevdev, ev, size * 64)) < size) {
            break;
        }
        if (ev[0].value != ' ' && ev[0].code != 2 && ev[1].code != 0) {
            handle_event(ev[1].code, seq, port, (int)ev[1].value);
        }
    }
    printf("Exiting.\n");
    result = ioctl(fevdev, EVIOCGRAB, 1);
    close(fevdev);
    return 0;
}
