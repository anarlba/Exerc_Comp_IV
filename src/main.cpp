#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>

// Configuração global
constexpr int NUM_JOGADORES = 4;
std::counting_semaphore<NUM_JOGADORES> cadeira_sem(NUM_JOGADORES - 1);
std::condition_variable music_cv;
std::mutex music_mutex;
std::mutex eliminacao_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};
std::vector<std::atomic<bool>> jogador_ativo(NUM_JOGADORES + 1, true); // Índices de 1 a N

// Classe principal do jogo
class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {}

    void iniciar_rodada() {
        std::unique_lock<std::mutex> lock(music_mutex);
        cadeiras--;
        musica_parada = false;
        std::cout << "\n--- Nova rodada! Cadeiras restantes: " << cadeiras << " ---\n";
        cadeira_sem = std::counting_semaphore<NUM_JOGADORES>(cadeiras);
    }

    void parar_musica() {
        {
            std::lock_guard<std::mutex> lock(music_mutex);
            musica_parada = true;
            std::cout << "\n>>> A música PAROU! Corram para as cadeiras!\n";
        }
        music_cv.notify_all();
    }

    void eliminar_jogador(int jogador_id) {
        std::lock_guard<std::mutex> lock(eliminacao_mutex);
        jogador_ativo[jogador_id] = false;
        std::cout << "Jogador " << jogador_id << " foi ELIMINADO!\n";
    }

    void exibir_estado() {
        std::lock_guard<std::mutex> lock(music_mutex);
        std::cout << "Rodada encerrada. Cadeiras disponíveis: " << cadeiras << "\n";
    }

private:
    int num_jogadores;
    int cadeiras;
};

// Classe do jogador
class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo) {}

    void tentar_ocupar_cadeira() {
        std::unique_lock<std::mutex> lock(music_mutex);
        music_cv.wait(lock, [] { return musica_parada.load(); });
        lock.unlock();

        std::cout << "Jogador " << id << " tentando sentar...\n";

        if (cadeira_sem.try_acquire_for(std::chrono::milliseconds(500))) {
            std::cout << "Jogador " << id << " CONSEGUIU uma cadeira!\n";
        } else {
            jogo.eliminar_jogador(id);
            jogo_ativo = false; // Encerra a thread deste jogador
        }
    }

    void joga() {
        while (jogador_ativo[id] && jogo_ativo) {
            tentar_ocupar_cadeira();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Jogador " << id << " saindo da thread.\n";
    }

private:
    int id;
    JogoDasCadeiras& jogo;
};

// Coordenador do jogo
class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    void iniciar_jogo() {
        int jogadores_restantes = NUM_JOGADORES;

        while (jogadores_restantes > 1) {
            std::this_thread::sleep_for(std::chrono::seconds(rand() % 3 + 2)); // Música tocando
            jogo.parar_musica();

            std::this_thread::sleep_for(std::chrono::seconds(2)); // Tempo para reação dos jogadores

            // Conta jogadores ativos
            jogadores_restantes = 0;
            for (int i = 1; i <= NUM_JOGADORES; ++i) {
                if (jogador_ativo[i]) {
                    jogadores_restantes++;
                }
            }

            if (jogadores_restantes == 1) {
                for (int i = 1; i <= NUM_JOGADORES; ++i) {
                    if (jogador_ativo[i]) {
                        std::cout << "\n*** Jogador " << i << " venceu o jogo! ***\n";
                    }
                }
                jogo_ativo = false;
                break;
            }

            jogo.exibir_estado();
            liberar_threads_eliminadas();
            jogo.iniciar_rodada();
        }
    }

    void liberar_threads_eliminadas() {
        cadeira_sem.release(NUM_JOGADORES - 1); // Garante que threads não bloqueiem indefinidamente
    }

private:
    JogoDasCadeiras& jogo;
};

// Função principal
int main() {
    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);
    std::vector<std::thread> jogadores;

    std::vector<Jogador> jogadores_objs;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores_objs.emplace_back(i, jogo);
    }

    for (int i = 0; i < NUM_JOGADORES; ++i) {
        jogadores.emplace_back(&Jogador::joga, &jogadores_objs[i]);
    }

    std::thread coordenador_thread(&Coordenador::iniciar_jogo, &coordenador);

    for (auto& t : jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }

    std::cout << "\nJogo das Cadeiras finalizado.\n";
    return 0;
}
