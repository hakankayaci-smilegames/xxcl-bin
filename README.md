# XXCL (Xbox XBE Compatibility Layer)

Zero-dependency 32-bit Native Linux loader for original Xbox (.xbe) executables.

## Durum (Nerede Kaldık?)
- **Mevcut Altyapı:**
  - XBE Header, Section Table ve Kernel Thunk tablosunu parse edebilen `xbe-inspect` aracı çalışıyor.
  - Obfuscated/gizli thunk tablolarını bulmak için `brute-forcer` devrede (Sega GT için başarıyla 121 ordinal hooklandı).
  - Proje saf **32-bit Native (i686)** Linux process'i olarak derleniyor. Böylece Xbox (x86) kodları hiçbir emülasyon maliyeti olmadan doğrudan çalıştırılabiliyor.
  - JIT (Trampoline) altyapısı hazır, kernel ordinal çağrıları `generic_stub`'a düşecek şekilde kancalandı.
  - SDL2 şimdilik devredışı bırakıldı (Native 32-bit testlerini hızlandırmak için).

- **Son Hata ve Çözümü:**
  - Oyun 64-bit modda yanlış opcode yorumladığı için çökmüştü. 32-bit'e geçirildi.
  - 32-bit'e geçince oyun Native olarak akmaya başladı ancak `0x106C0F` adresinde `0x0` null pointer yedi. Sebebi: Oyun, Base Address (`0x10000`) üzerinden `CertificateAddress` (0x10118) okumaya çalışıyordu ancak biz sadece Section'ları RAM'e yüklemiştik, XBE Header'ı yüklememiştik.
  - **Çözüm:** `loader.cpp` dosyasına XBE Header'ı (ilk 4KB bloğu) `0x10000` adresine kopyalayan kodu ekledik.

## Sonraki Adım (Next Steps)
Projeyi derleyip Sega GT'yi çalıştır ve oyunun 32-bit modda header kontrolünü geçip `[KERNEL HOOK] Cagrilan Ordinal: X` stub loglarına düşüp düşmediğini test et:

```bash
cd xbe-inspect/build
rm -rf *
cmake ..
make
./xbe-inspect /dosya/yolu/sega_gt.xbe --run
```

## Derleme Gereksinimleri
- CachyOS / Arch Linux üzerinde 32-bit derleme kütüphaneleri (gcc-multilib vb.)
