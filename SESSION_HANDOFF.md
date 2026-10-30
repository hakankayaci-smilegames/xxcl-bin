# XXCL Oturum Devir Notu

Son güncelleme: 30 Ekim 2026

Çalışma dizini: `/home/hakan/Desktop/xxcl-bin`

## Güncel durum

Son iş tamamlandı: GP DSP'nin `MOVEC #xxxxxx,M0–M7` komutları,
16 bit register genişliği ve iki program word tüketimiyle destekleniyor.
Temiz HDD koşusunda eski `PC=0x070 / P=0x05F420` noktası geçildi.
Son gözlenen checkpoint:

```text
DSP instructions=519061 PC=0x072
P[PC..PC+2]=0461A0/0462A0/0463A0
GPU PFIFO DMA GET=0470D000 PUT=0470D000
```

İlk oyun görüntüsü henüz çizilmiyor. Son değişikliğin oyun tarafındaki ses
beklemesini tamamen geçirdiği doğrulanmış değil. Sonraki geliştirme, DSP
`0x0461A0` komutunu üretici belgesinden çözmekle başlamalı; bu oturumda
o komutun uygulamasına başlanmadı.

## Bu oturumda tamamlananlar

- Bekleyen eski tanılama dosyaları ve ikili dosyalar kullanıcı isteğiyle
  `4ac1b75` commit'inde saklandı ve gönderildi.
- GP scratch tablosunun adresi ve sınır değişiklikleri çalışan DSP'ye aktarılıyor.
- Bellek eşlemesi henüz hazır değilse aktif DMA sonraki çalışma turunda yeniden
  deneniyor; gerçek aktarım tamamlanmadan EOL bildirilmiyor. STOP, ABORT ve
  bootstrap bekleyen işi iptal ediyor.
- `CMP X0,A` artık X0'ın gerçek değerini karşılaştırıyor. Önceki kod,
  pozitif X0 değerini kaybedip `3 == 0` sonucunu üretebiliyordu.
- Doğrudan X/Y veri belleğindeki biti sınayan `JCLR/JSET` komutları destekleniyor.
- Immediate `MOVEC` için M0 ve M7, 16 bit daraltma, PC ilerlemesi ve durum
  bayraklarının korunması test edildi.

## Doğrulama ve çalıştırma

Derleme ve 8/8 CTest hedefi başarılı:

```bash
cmake -S xbe-inspect -B xbe-inspect/build
cmake --build xbe-inspect/build -j"$(nproc)"
ctest --test-dir xbe-inspect/build --output-on-failure
```

`xxcl` komutu `/home/hakan/.local/bin/xxcl` üzerinden depo kökündeki wrapper'a,
wrapper da `xbe-inspect/build/xbe-inspect` dosyasına gider. Depodaki
`xbe-inspect/xbe-inspect` ikili dosyası bu komutun hedefi değildir.

İlk açılışın reboot isteğini karşılamak için iki çalıştırmada aynı izole kökü kullan:

```bash
XXCL_GAME_XBE='/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe'
XXCL_RUN_ROOT=$(mktemp -d /tmp/xxcl-run.XXXXXX)
./xxcl "$XXCL_GAME_XBE" --run --hdd-root "$XXCL_RUN_ROOT"
./xxcl "$XXCL_GAME_XBE" --run --hdd-root "$XXCL_RUN_ROOT"
```

Son doğrulama kökü: `/tmp/xxcl-oct30-movec.DDXckC`.
Loglar: `/tmp/xxcl-oct30-movec-first.log` ve
`/tmp/xxcl-oct30-movec-second.log`.
İlk koşu reboot isteyip 0 ile çıktı; ikinci koşu 15 saniyelik test sınırında
124 ile durduruldu. Süre sınırı oyunun tamamlandığı anlamına gelmez.
Geçici dosyalar sonraki oturumda bulunmayabilir.

## Mimari sınırlar

- Sega GT yalnızca entegrasyon örneğidir; oyun adına, title ID'ye veya gözlenen
  adreslere bağlı bypass eklenmez.
- Xbox kernel export imzaları ve `__stdcall` düzeni korunur.
- Scratch aktarımında SGE tablosu ve hedef aralık kopyalamadan önce doğrulanır.
- GPU GET, yalnız gerçekten işlenen komut nedeniyle ilerler.
- SDL işlemleri ana/video thread'inde kalır; host FPS, oyun FPS'i değildir.
- Entegrasyon koşularında geçici `--hdd-root` kullanılır.
- Kullanıcının tercihi: sade Türkçe açıklama, izin sorarak duraksamadan ilerleme,
  doğrulanmış değişikliklerde commit ve push.
- Yeni oturum önce `git status --short --branch` ve `git log -5 --oneline`
  ile gerçek durumu kontrol etmelidir.

## Belgeler ve sıradaki iş

Güncel checkpoint için `STATUS.md`, mimari sınırlar için
`PROJECT_ARCHITECTURE.md`, kullanım için `README.md` okunmalı.
30 Ekim planları `docs/superpowers/plans/` altında bulunur.
Eski devir notunun tarihsel ayrıntıları `4ac1b75` commit'inde korunur;
oradaki eski PC ve 7 test bilgileri güncel değildir.

Sonraki iş: `PC=0x072 / P=0x0461A0` komutunu DSP56300 belgesiyle çöz,
oyun bağımsız bir test ekle, genel davranışı uygula ve aynı izole açılışla
yeni checkpoint'i doğrula. Uzun vadeli hedef gerçek guest GPU clear komutunun
SDL penceresinde ilk görünür sonucu üretmesidir.
