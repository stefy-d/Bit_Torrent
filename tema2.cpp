#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <algorithm>
#include <random>
#include <queue>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

// etichete pentru tipurile de actiuni ale clientilor 
#define REQUEST 0
#define FILE_COMPLETED 1
#define DOWNLOAD_ALL_DONE 2
#define UPLOAD 3
#define CLOSE_ALL 4

// tag-uri pentru mesajele trimise/primite
#define TAG_SEND_INFO 10
#define TAG_ALL_FILES 11
#define TAG_ACK 12
#define TAG_TYPE 13
#define TAG_SWARM 14
#define TAG_CLOSE_DOWNLOAD 15
#define TAG_UPLOAD 16
#define TAG_FILE_COMPLETED 17
#define TAG_CONFIRMATION 18


int N;  // numarul de clienti
int active_clients; // numarul de clienti activi

// structura pentru felul in care retin datele despre fisiere
struct files {
    std::vector<std::string> hashes;    // hash-urile
    int num_hashes;     // numarul de hash-uri detinute
};

// lista de seeds/peers pentru fiecare fisier
struct client_types {
    std::set<int> peers;
    std::set<int> seeds;
};

// informatii despre un client
struct clients {
    std::map<std::string, struct files> client_files;   // fisierele pe care le are
    std::vector<std::string> wanted_files;  // fisierele pe care le vrea citite la initializare
    int num_client_files, num_wanted_files; // numarul de fisiere
    std::map<std::string, struct files> all_files;  // datele despre toate fisierele
};

struct clients* clients;    // clientii

std::map<std::string, struct files> all_files;  // mapa pentru toate fisierele care exista
std::map<std::string, struct client_types> files_info;  // swarm-ul detinut de tracker

// citire fisiere initializare
void read_files(int rank) {
    
    std::string filename;
    filename = "in" + std::to_string(rank) + ".txt";
    std::ifstream fin(filename);

    if (!fin.is_open()) {
        printf("Eroare la deschiderea fisierului %s\n", filename.c_str());
        exit(-1);
    }

    // citeste numarul de fisiere detinute de client, le retine clientul si le trimite si la tracker
    fin >> clients[rank].num_client_files;
    MPI_Send(&clients[rank].num_client_files, 1, MPI_INT, TRACKER_RANK, TAG_SEND_INFO, MPI_COMM_WORLD);

    // citeste fisierele detinute de client, le retine clientul si le trimite si la tracker
    std::string file;
    for (int i = 0; i < clients[rank].num_client_files; i++) {
        fin >> file;
        MPI_Send(file.c_str(), file.size() + 1, MPI_CHAR, TRACKER_RANK, TAG_SEND_INFO, MPI_COMM_WORLD);

        struct files aux;
        // citeste numarul de hash-uri pentru fisierul curent, le retine clientul si le si trimite la tracker       
        fin >> aux.num_hashes;
        MPI_Send(&aux.num_hashes, 1, MPI_INT, TRACKER_RANK, TAG_SEND_INFO, MPI_COMM_WORLD);

        for (int j = 0; j < aux.num_hashes; j++) {
            std::string hash;
            fin >> hash;
            aux.hashes.push_back(hash);

            MPI_Send(aux.hashes[j].c_str(), aux.hashes[j].size() + 1, MPI_CHAR, TRACKER_RANK, TAG_SEND_INFO, MPI_COMM_WORLD);
        }

        clients[rank].client_files[file] = aux;

    }

    // citeste numarul de fisiere dorite de client, dar pe acestea nu le trimite la tracker
    fin >> clients[rank].num_wanted_files;
    for (int i = 0; i < clients[rank].num_wanted_files; i++) {
        fin >> file;
        clients[rank].wanted_files.push_back(file);

        struct files aux;
        aux.num_hashes = 0;
        aux.hashes.resize(clients[rank].all_files[file].num_hashes, "");
        clients[rank].client_files[file] = aux;
    }
}

// primire info despre toate fisierele existente
void get_data(int rank) {
    
    MPI_Status status;
    int num_files;
    // primesc numarul total de fisiere
    MPI_Recv(&num_files, 1, MPI_INT, TRACKER_RANK, TAG_ALL_FILES, MPI_COMM_WORLD, &status);

    // pentru fiecare fisier primeasc numele, numarul total de hash-uri si hash-urile
    for (int i = 0; i < num_files; i++) {
        char filename_buffer[MAX_FILENAME + 1];
        MPI_Recv(filename_buffer, sizeof(filename_buffer), MPI_CHAR, TRACKER_RANK, TAG_ALL_FILES, MPI_COMM_WORLD, &status);
        std::string filename(filename_buffer);

        int num_hashes;
        MPI_Recv(&num_hashes, 1, MPI_INT, TRACKER_RANK, TAG_ALL_FILES, MPI_COMM_WORLD, &status);

        struct files file_info;
        file_info.num_hashes = num_hashes;

        for (int j = 0; j < num_hashes; j++) {
            char hash_buffer[HASH_SIZE + 1];
            MPI_Recv(hash_buffer, sizeof(hash_buffer), MPI_CHAR, TRACKER_RANK, TAG_ALL_FILES, MPI_COMM_WORLD, &status);
            file_info.hashes.push_back(std::string(hash_buffer));
        }

        // retin informatiile la client 
        clients[rank].all_files[filename] = file_info;
    }

}

// trimitere swarm solicitat pentru un fisier
void send_swarm(int rank, std::string file) {

    // numarul total de clienti care detin fisierul complet sau nu
    int num_peers = files_info[file].peers.size() + files_info[file].seeds.size();
    MPI_Send(&num_peers, 1, MPI_INT, rank, TAG_SWARM, MPI_COMM_WORLD);

    // trmit seeds
    for (const auto& seed : files_info[file].seeds) {
        MPI_Send(&seed, 1, MPI_INT, rank, TAG_SWARM, MPI_COMM_WORLD);
    }

    // trimit peers
    for (const auto& peer : files_info[file].peers) {
        MPI_Send(&peer, 1, MPI_INT, rank, TAG_SWARM, MPI_COMM_WORLD);
    }

    // daca clientul care a solicitat swarm-ul nu este seed pentru fisier, il adaug ca peer
    if (files_info[file].seeds.find(rank) == files_info[file].seeds.end()) {
        files_info[file].peers.insert(rank);
    }
}

// randomizez ordinea clientilor din swarm-ul unui fisier
void shuffle_queue(std::queue<int>& q) {
    
    // pun elementele din coada in vector
    std::vector<int> elements;
    while (!q.empty()) {
        elements.push_back(q.front());
        q.pop();
    }

    // amestec elementele din vector
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(elements.begin(), elements.end(), g);

    // pun elementele inapoi in coada
    for (const auto& elem : elements) {
        q.push(elem);
    }

}

// cer swarm-ul unui fisier de la tracker
std::queue<int> request_swarm(int rank, const std::string& filename) {

    MPI_Status status;
    int type = REQUEST; // ca sa stie tracker-ul ce vreau sa faca
    int num_peers;
    std::queue<int> peer_queue;

    // trimite cerere pentru swarm-ul fisierului
    MPI_Send(&type, 1, MPI_INT, TRACKER_RANK, TAG_TYPE, MPI_COMM_WORLD);
    MPI_Send(filename.c_str(), filename.size() + 1, MPI_CHAR, TRACKER_RANK, TAG_SWARM, MPI_COMM_WORLD);

    // primesc numarul de peers
    MPI_Recv(&num_peers, 1, MPI_INT, TRACKER_RANK, TAG_SWARM, MPI_COMM_WORLD, &status);

    // primesc lista de peers si ii pun intr-o coada
    int peer;
    for (int i = 0; i < num_peers; i++) {
        MPI_Recv(&peer, 1, MPI_INT, TRACKER_RANK, TAG_SWARM, MPI_COMM_WORLD, &status);
        peer_queue.push(peer);
    }

    // amestec coada pe care am construit-o
    shuffle_queue(peer_queue);

    // returnez coada
    return peer_queue;
}

void tracker(int numtasks, int rank) {

    MPI_Status status;

    for (int i = 1; i < numtasks; i++) {

        // primeste numarul de fisiere detinute de clientul i
        int num_client_files;
        MPI_Recv(&num_client_files, 1, MPI_INT, i, TAG_SEND_INFO, MPI_COMM_WORLD, &status);

        // pentru fiecare fisier primesc numele, numarul de hash-uri si hash-urile
        for (int j = 0; j < num_client_files; j++) {
            char filename_buffer[MAX_FILENAME + 1];
            MPI_Recv(filename_buffer, sizeof(filename_buffer), MPI_CHAR, i, TAG_SEND_INFO, MPI_COMM_WORLD, &status);
            std::string filename(filename_buffer);

            int num_hashes;
            MPI_Recv(&num_hashes, 1, MPI_INT, i, TAG_SEND_INFO, MPI_COMM_WORLD, &status);

            struct files aux_info;
            aux_info.num_hashes = num_hashes;

            for (int k = 0; k < num_hashes; k++) {
                char hash_buffer[HASH_SIZE + 1];
                MPI_Recv(hash_buffer, sizeof(hash_buffer), MPI_CHAR, i, TAG_SEND_INFO, MPI_COMM_WORLD, &status);
                aux_info.hashes.push_back(std::string(hash_buffer));
            }

            // daca fisierul primit nu exista in mapa cu toate fisierele il adaug
            if (all_files.find(filename) == all_files.end()) {
                all_files[filename] = aux_info;
            } 

            // marchez clientul de la care s-a primit fisierul ca seed
            files_info[filename].seeds.insert(i);
        }
    }    

    // trimite fiecarui client informatiile despre toate fisierele
    for (int i = 1; i < numtasks; i++) {
        int num_files = all_files.size();
        MPI_Send(&num_files, 1, MPI_INT, i, TAG_ALL_FILES, MPI_COMM_WORLD);

        for (const auto& [filename, file_info] : all_files) {
            MPI_Send(filename.c_str(), filename.size() + 1, MPI_CHAR, i, TAG_ALL_FILES, MPI_COMM_WORLD);
            MPI_Send(&file_info.num_hashes, 1, MPI_INT, i, TAG_ALL_FILES, MPI_COMM_WORLD);
            for (const auto& hash : file_info.hashes) {
                MPI_Send(hash.c_str(), hash.size() + 1, MPI_CHAR, i, TAG_ALL_FILES, MPI_COMM_WORLD);
            }
        }
    }

    //trimite ack fiecarui client pentru a putea incepe descarcarea
    char ack[] = "ACK";
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(ack, sizeof(ack), MPI_CHAR, i, TAG_ACK, MPI_COMM_WORLD);
    }  

    int type;   // tipul mesajului primit de tracker
    int flag;

    while (active_clients) {

        // verific daca e vreun mesaj pentru mine
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_TYPE, MPI_COMM_WORLD, &flag, &status);
        if (flag) {

            // primesc tipul mesajului
            MPI_Recv(&type, 1, MPI_INT, MPI_ANY_SOURCE, TAG_TYPE, MPI_COMM_WORLD, &status);
            
            switch (type) {
                case REQUEST: {

                    // primesc numele fisierului dorit de client si ii trimit swarm-ul
                    char filename[MAX_FILENAME + 1];
                    MPI_Recv(filename, sizeof(filename), MPI_CHAR, status.MPI_SOURCE, TAG_SWARM, MPI_COMM_WORLD, &status);
                    std::string file(filename);
                    send_swarm(status.MPI_SOURCE, file);                   
                    break;
                }

                case FILE_COMPLETED: {

                    // primesc numele fisierului care a fost descarcat complet
                    char filename[MAX_FILENAME + 1];
                    MPI_Recv(filename, sizeof(filename), MPI_CHAR, status.MPI_SOURCE, TAG_FILE_COMPLETED, MPI_COMM_WORLD, &status);

                    // sterg clientul de la peers si il marchez ca seed
                    files_info[filename].peers.erase(status.MPI_SOURCE);
                    files_info[filename].seeds.insert(status.MPI_SOURCE);
                    break;
                }

                case DOWNLOAD_ALL_DONE: {

                    // un client a terminat de descarcat toate fisierele si trimit mesaj sa-si inchida thread-ul download
                    active_clients--;
                    MPI_Send(&type, 1, MPI_INT, status.MPI_SOURCE, TAG_CLOSE_DOWNLOAD, MPI_COMM_WORLD);
                    break;
                }
            }
        }

    }

    // trimit mesaje de inchidere si pentru thread-urile upload
    for (int i = 1; i < numtasks; i++) {
        type = CLOSE_ALL;
        MPI_Send(&type, 1, MPI_INT, i, TAG_UPLOAD, MPI_COMM_WORLD);
    }   
}

void *upload_thread_func(void *arg) {
    int rank = *(int*) arg;
    MPI_Status status;
    int type;
    int flag;
    int ack = 1;
    int nack = 0;

    while (true) {

        // verific daca am de primit vreun mesaj
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_UPLOAD, MPI_COMM_WORLD, &flag, &status);

        if (flag) {

            // primesc tipul mesajului
            MPI_Recv(&type, 1, MPI_INT, MPI_ANY_SOURCE, TAG_UPLOAD, MPI_COMM_WORLD, &status);

            if (type == UPLOAD) {

                // cerere de upload: primesc numele fisierului si indexul hash-ului dorit
                char filename[MAX_FILENAME + 1];
                int hash_idx;

                MPI_Recv(filename, sizeof(filename), MPI_CHAR, status.MPI_SOURCE, TAG_UPLOAD, MPI_COMM_WORLD, &status);
                MPI_Recv(&hash_idx, 1, MPI_INT, status.MPI_SOURCE, TAG_UPLOAD, MPI_COMM_WORLD, &status);
                
                std::string file(filename);

                // verific daca am hash-ul si trimit un mesaj corespunzator
                if (hash_idx < clients[rank].client_files[filename].num_hashes) {
                    MPI_Send(&ack, 1, MPI_INT, status.MPI_SOURCE, TAG_CONFIRMATION, MPI_COMM_WORLD);
                } else {
                    MPI_Send(&nack, 1, MPI_INT, status.MPI_SOURCE, TAG_CONFIRMATION, MPI_COMM_WORLD);
                }

            } else {
                // mesaj de inchidere thread
                if (type == CLOSE_ALL) {                
                    break;
                }
                
            }

        }

    }

    return NULL;
}

void *download_thread_func(void *arg){

    int rank = *(int*) arg;
    MPI_Status status;
    int type;

    // iau fiecare fisier pe rand si il descarc
    for (auto& filename : clients[rank].wanted_files) {

        // inati de toate cer swarm-ul fisierului de la tracker
        std::queue<int> peer_queue = request_swarm(rank, filename);

        int prev = clients[rank].client_files[filename].num_hashes; // cate hash-uri aveam la ultima actualizare
        int response;   // mesajul pe care l primesc de la peer daca are sau nu ce ii cer

        // completez fisierul cu toate hash-urile
        while (clients[rank].client_files[filename].num_hashes < clients[rank].all_files[filename].num_hashes) {

            // la fiecare 10 fisiere primite actualizez swarm-ul
            if (clients[rank].client_files[filename].num_hashes % 10 == 0 && clients[rank].client_files[filename].num_hashes != prev) {              
                peer_queue = {};
                peer_queue = request_swarm(rank, filename);
                prev = clients[rank].client_files[filename].num_hashes;
            }

            // selectez peer-ul, ma asigur ca nu sunt chiar eu si il pun la finalul cozii
            int peer = peer_queue.front();
            peer_queue.pop();
            while (peer == rank) {
                peer_queue.push(peer);
                peer = peer_queue.front();
                peer_queue.pop();
                
            }
            peer_queue.push(peer);

            // trimit cerere de upload la peer-ul selectat
            type = UPLOAD;
            MPI_Send(&type, 1, MPI_INT, peer, TAG_UPLOAD, MPI_COMM_WORLD);
            MPI_Send(filename.c_str(), filename.size() + 1, MPI_CHAR, peer, TAG_UPLOAD, MPI_COMM_WORLD);
            MPI_Send(&clients[rank].client_files[filename].num_hashes, 1, MPI_INT, peer, TAG_UPLOAD, MPI_COMM_WORLD);

            // primesc raspunsul            
            MPI_Recv(&response, 1, MPI_INT, peer, TAG_CONFIRMATION, MPI_COMM_WORLD, &status);

            // verfic daca am primit raspuns afirmativ ca sa trec la urmatorul hash
            if (response == 1)
                clients[rank].client_files[filename].num_hashes++;
        }

        // cand termin de descarcat fisierul anunt tracker-ul
        type = FILE_COMPLETED;
        MPI_Send(&type, 1, MPI_INT, TRACKER_RANK, TAG_TYPE, MPI_COMM_WORLD);
        MPI_Send(filename.c_str(), filename.size() + 1, MPI_CHAR, TRACKER_RANK, TAG_FILE_COMPLETED, MPI_COMM_WORLD);

        // scrie in fisierul de iesire
        std::string out_filename = "client" + std::to_string(rank) + "_" + filename;
        std::ofstream fout(out_filename);
        for (const auto& hash : clients[rank].all_files[filename].hashes) {
            fout << hash << "\n";
        }
    }

    // cand termin de descarcat toate fisierele anunt tracker-ul si astept permisiune sa inchid thread-ul
    type = DOWNLOAD_ALL_DONE;
    MPI_Send(&type, 1, MPI_INT, TRACKER_RANK, TAG_TYPE, MPI_COMM_WORLD);
    MPI_Recv(&type, 1, MPI_INT, TRACKER_RANK, TAG_CLOSE_DOWNLOAD, MPI_COMM_WORLD, &status);
    
    return NULL;
}

void peer(int numtasks, int rank) {
    
    read_files(rank);  // citeste fisierele detinute de client si trimite informatiile la tracker
    get_data(rank);     // primeste informatiile despre toate fisierele de la tracker

    // primesc ack de la tracker ca sa pot incepe descarcarea
    char ack[4];
    MPI_Status status;
    MPI_Recv(ack, sizeof(ack), MPI_CHAR, TRACKER_RANK, TAG_ACK, MPI_COMM_WORLD, &status);

    int r;
    pthread_t download_thread, upload_thread;
    void *thread_return;

    // deschid thread-ul de download
    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }
    
    // deschid thread-ul de upload
    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }
    
    // astept sa inchid download-ul
    r = pthread_join(download_thread, &thread_return);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    // astept sa inchid upload-ul
    r = pthread_join(upload_thread, &thread_return);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }

}

int main(int argc, char* argv[]) {
    int numtasks, rank;
    int provided;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }   
    
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    N = numtasks;
    clients = new struct clients[N];    // clientii
    active_clients = N - 1;     // clientii activi

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();

    delete[] clients;
}
