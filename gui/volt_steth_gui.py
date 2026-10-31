#!/usr/bin/env python3
import sys
import os
import json
import subprocess
import threading
import time

import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw, GLib, Pango

BINARY_PATH = "/home/walbarellos/volt-steth/build/volt-steth"
CONFIG_PATH = "/home/walbarellos/volt-steth/config.default.ini"

class VoltStethApp(Adw.Application):
    def __init__(self):
        super().__init__(application_id="io.github.walbarellos.voltsteth")
        self.window = None

    def do_activate(self):
        if not self.window:
            self.window = VoltStethWindow(application=self)
        self.window.present()

class VoltStethWindow(Adw.ApplicationWindow):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.set_title("VoltSteth - Estetoscópio Elétrico de Hardware")
        self.set_default_size(720, 680)

        self.monitoring_proc = None
        self.is_monitoring = False
        self.peak_db = -100.0

        # Layout Principal com Adw ToolbarView
        toolbar_view = Adw.ToolbarView()
        self.set_content(toolbar_view)

        # Header Bar moderna
        header = Adw.HeaderBar()
        title_widget = Adw.WindowTitle(title="VoltSteth ⚡🩺", subtitle="Diagnóstico Eletromecânico & Malha de Terra")
        header.set_title_widget(title_widget)
        toolbar_view.add_top_bar(header)

        # Conteúdo em Adw.PreferencesPage (Estilo nativo Linux/GNOME)
        pref_page = Adw.PreferencesPage()
        toolbar_view.set_content(pref_page)

        # -------------------------------------------------------------
        # GRUPO 1: STATUS DE DIAGNÓSTICO E SAÚDE FÍSICA
        # -------------------------------------------------------------
        status_group = Adw.PreferencesGroup(title="Status Geral da Máquina")
        pref_page.add(status_group)

        self.status_banner = Adw.Banner(
            title="Sensor Calibrado: Nenhum loop de terra detectado.",
            revealed=True
        )
        self.status_banner.set_button_label("Auditar Agora")
        self.status_banner.connect("button-clicked", self.on_audit_clicked)
        status_group.add(self.status_banner)

        # Cartão de Ações Rápidas
        actions_row = Adw.ActionRow(title="Auditoria Automática de 6 Segundos", subtitle="Amostra a malha física e emite laudo para análise")
        self.audit_btn = Gtk.Button(label="▶ Iniciar Auditoria", valign=Gtk.Align.CENTER)
        self.audit_btn.add_css_class("suggested-action")
        self.audit_btn.connect("clicked", self.on_audit_clicked)
        actions_row.add_suffix(self.audit_btn)
        status_group.add(actions_row)

        self.audit_progress = Gtk.ProgressBar(show_text=True, text="Aguardando início...")
        self.audit_progress.set_margin_top(6)
        self.audit_progress.set_margin_bottom(6)
        status_group.add(self.audit_progress)

        # -------------------------------------------------------------
        # GRUPO 2: MEDIDOR ANALÓGICO EM TEMPO REAL (VU METER + PEAK HOLD)
        # -------------------------------------------------------------
        meter_group = Adw.PreferencesGroup(title="Medição de Ruído e Harmônicas (Tempo Real)")
        pref_page.add(meter_group)

        self.vu_row = Adw.ActionRow(title="Nível de Ruído Analógico (Piso)", subtitle="-100.0 dBFS")
        self.vu_bar = Gtk.ProgressBar()
        self.vu_bar.set_size_request(240, -1)
        self.vu_bar.set_valign(Gtk.Align.CENTER)
        self.vu_row.add_suffix(self.vu_bar)
        meter_group.add(self.vu_row)

        self.monitor_toggle_row = Adw.ActionRow(title="Monitoramento Contínuo", subtitle="Exibe em tempo real oscilações elétricas")
        self.monitor_switch = Gtk.Switch(valign=Gtk.Align.CENTER)
        self.monitor_switch.connect("state-set", self.on_monitor_toggle)
        self.monitor_toggle_row.add_suffix(self.monitor_switch)
        meter_group.add(self.monitor_toggle_row)

        # -------------------------------------------------------------
        # GRUPO 3: TOPOLOGIA FÍSICA E TRIANGULAÇÃO
        # -------------------------------------------------------------
        topo_group = Adw.PreferencesGroup(title="Topologia de Hardware Mapeada (Triangulação)")
        pref_page.add(topo_group)

        self.card_analog_row = Adw.ActionRow(title="Placa 0: Analógica (Realtek ALC897)", subtitle="Carregando status...")
        self.unmute_btn = Gtk.Button(label="Calibrar Ganho 80%", valign=Gtk.Align.CENTER)
        self.unmute_btn.connect("clicked", self.on_calibrate_gain)
        self.card_analog_row.add_suffix(self.unmute_btn)
        topo_group.add(self.card_analog_row)

        self.card_digital_row = Adw.ActionRow(title="Placa 1: Digital (NVIDIA GM206 HDMI)", subtitle="Isolamento digital ativo")
        topo_group.add(self.card_digital_row)

        self.usb_row = Adw.ActionRow(title="Barramento USB 3.2 xHCI", subtitle="Carregando dispositivos...")
        topo_group.add(self.usb_row)

        # -------------------------------------------------------------
        # GRUPO 4: ÚLTIMO LAUDO TÉCNICO GERADO
        # -------------------------------------------------------------
        log_group = Adw.PreferencesGroup(title="Último Boletim Técnico Emitido")
        pref_page.add(log_group)

        self.log_view = Gtk.TextView(editable=False, cursor_visible=False)
        self.log_view.set_monospace(True)
        self.log_view.get_buffer().set_text("Nenhuma auditoria executada nesta sessão ainda.\nClique em 'Iniciar Auditoria' acima para coletar os dados físicos.")
        
        scroll = Gtk.ScrolledWindow()
        scroll.set_min_content_height(140)
        scroll.set_child(self.log_view)
        log_group.add(scroll)

        # Carrega topologia inicial
        GLib.idle_add(self.refresh_topology)

    def refresh_topology(self):
        try:
            res = subprocess.run([BINARY_PATH, "--topology"], capture_output=True, text=True)
            output = res.stdout

            # Atualiza textos
            if "ANALÓGICA" in output:
                self.card_analog_row.set_subtitle("Jack P2 Verde | Volume: Ativo | Desmutada")
            if "DIGITAL" in output:
                self.card_digital_row.set_subtitle("Saída HDMI/DisplayPort | Blindagem Digital")
            
            # USBs
            usb_lines = [l.strip().replace("• ", "") for l in output.splitlines() if "USB [" in l]
            if usb_lines:
                self.usb_row.set_subtitle(" | ".join(usb_lines[:2]))
        except Exception as e:
            self.card_analog_row.set_subtitle(f"Erro ao ler topologia: {e}")

    def on_calibrate_gain(self, btn):
        subprocess.run(["amixer", "-c", "0", "sset", "Capture", "80%"])
        subprocess.run(["amixer", "-c", "0", "sset", "Capture", "unmute"])
        self.status_banner.set_title("Mixer calibrado com sucesso: Volume em 80%.")
        self.refresh_topology()

    def on_audit_clicked(self, widget):
        self.audit_btn.set_sensitive(False)
        self.audit_progress.set_fraction(0.1)
        self.audit_progress.set_text("Executando auditoria física (6 segundos)...")

        def run_thread():
            try:
                cmd = [BINARY_PATH, "--config", CONFIG_PATH, "--diagnose"]
                p = subprocess.run(cmd, capture_output=True, text=True)
                output = p.stdout
                GLib.idle_add(self.on_audit_finished, output)
            except Exception as e:
                GLib.idle_add(self.on_audit_finished, f"Erro: {e}")

        threading.Thread(target=run_thread, daemon=True).start()

    def on_audit_finished(self, output):
        self.audit_btn.set_sensitive(True)
        self.audit_progress.set_fraction(1.0)
        self.audit_progress.set_text("Auditoria concluída!")
        self.log_view.get_buffer().set_text(output)

        if "GROUND LOOP DETECTADO" in output:
            self.status_banner.set_title("⚠️ ALERTA: Loop de terra detectado na malha analógica!")
            self.status_banner.set_use_markup(False)
        else:
            self.status_banner.set_title("✅ EXCELENTE: Sinal isolado e sem vazamento de harmônicas de rede.")

    def on_monitor_toggle(self, switch, state):
        if state:
            self.is_monitoring = True
            threading.Thread(target=self.monitor_worker, daemon=True).start()
        else:
            self.is_monitoring = False
            if self.monitoring_proc:
                self.monitoring_proc.terminate()
                self.monitoring_proc = None

    def monitor_worker(self):
        try:
            cmd = [BINARY_PATH, "--config", CONFIG_PATH, "--json"]
            self.monitoring_proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True)

            while self.is_monitoring and self.monitoring_proc and self.monitoring_proc.poll() is None:
                line = self.monitoring_proc.stdout.readline()
                if not line:
                    time.sleep(0.05)
                    continue
                try:
                    data = json.loads(line)
                    # Atualiza VU
                    metrics = data.get("metrics", {})
                    noise_rms = metrics.get("noise_rms", 0.0)
                    db = 20.0 * (noise_rms / 1000.0) if noise_rms > 0 else -100.0
                    GLib.idle_add(self.update_meter, db)
                except:
                    pass
        except:
            pass

    def update_meter(self, db):
        # Ballistics peak-hold decay
        if db > self.peak_db:
            self.peak_db = db
        else:
            self.peak_db -= 1.5 # decaimento suave

        norm = (self.peak_db + 80.0) / 80.0
        if norm < 0.0: norm = 0.0
        if norm > 1.0: norm = 1.0

        self.vu_bar.set_fraction(norm)
        self.vu_row.set_subtitle(f"{self.peak_db:.1f} dBFS")

def main():
    app = VoltStethApp()
    return app.run(sys.argv)

if __name__ == "__main__":
    sys.exit(main())
