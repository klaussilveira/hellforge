pkgname=hellforge
pkgver=1.0.0
pkgrel=1
pkgdesc="Level Editor for idTech4-based games (Doom 3, dhewm3, RBDOOM-3-BFG)"
arch=("x86_64")
url="https://github.com/klaussilveira/hellforge"
license=("GPL")
depends=(wxgtk2 ftgl glew freealut libvorbis python libsigc++ eigen)
makedepends=(cmake git)
source=("$pkgname::git+https://github.com/klaussilveira/hellforge.git#tag=$pkgver")
md5sums=("SKIP")

build() {
	cd "$pkgname"
	cmake .
	make
}

package() {
	cd "$pkgname"
	make DESTDIR="$pkgdir/" install
}
