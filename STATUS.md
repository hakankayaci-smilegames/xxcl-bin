# XXCL - Geliştirme Durumu (Sega GT 2002)

## Kaldığımız Yer
- **DVD Güvenlik Doğrulaması Aşıldı:** Oyunun `\Device\CdRom0` üzerinden yolladığı SCSI `MODE SENSE (10)` Page `0x3E` (Xbox Medya Doğrulaması) komutu yakalandı ve başarılı (`Authenticated = 1`, `Booktype = 0x0D`) olarak mocklandı.
- **VFS (Virtual File System) ve HDD Atlatması:** Linux dizinlerini (`./hdd/D`, `./hdd/E`) gösteren VFS çeviricisi eklendi. Oyunun HDD kontrolünü geçmesi için `NtQueryVolumeInformationFile` (Ordinal 218) ile 16KB cluster boyutu (Sectors:32 * Bytes:512) simüle edildi.
- **Sürücü Sembolik Bağları:** `IoCreateSymbolicLink` (Ordinal 67) uygulandı. Oyun başarıyla `\??\D:` adresini `\Device\CdRom0` dizinine bağladı.
- **Son Duraklama Noktası:** Cihazları ve diskleri bağlayan oyun, sürücülerdeki ilk dosyaya erişmek için **Ordinal 190 (`NtCreateFile`)** fonksiyonunu çağırdı ve sistem güvenli bir şekilde beklemeye geçti.

## Sonraki Muhtemel Görev
1. **Ordinal 190 (`NtCreateFile`) Fonksiyonunun Yazılması:** 
   Oyunun `D:\` (Oyun dataları) veya `E:\UDATA` (Save/Profil) üzerinden açmak istediği ilk dosyaları VFS üzerinden Linux `open()` çağrılarına bağlamak.
2. **Erişim Bayraklarının Çevirisi:** 
   `NtCreateFile`'ın aldığı NT parametrelerini (`CreateDisposition`, `DesiredAccess`, `ShareAccess`) POSIX eşdeğerlerine (`O_CREAT`, `O_TRUNC`, `O_RDWR` vb.) uyumlu şekilde çevirecek bir haritalama katmanı kurmak.
