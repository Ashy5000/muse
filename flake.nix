{
  description = "A hobby operating system written in Zig.";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/04607e1165ac22c5fde6dcc54c9e0b3c0487c555";
  inputs.zig.url = "github:mitchellh/zig-overlay";

  outputs = { self, nixpkgs, zig }:
  let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
  in {
    packages.${system}.default = pkgs.mkShellNoCC {
      packages = [
        zig.packages.${system}.master-2026-06-16
	pkgs.grub2_efi
	pkgs.gptfdisk
	pkgs.dosfstools
	pkgs.mtools
	pkgs.qemu
      ];

      FONT_PATH = pkgs.fetchurl {
        url = "https://github.com/talamus/solarize-12x29-psf/raw/refs/heads/master/Solarize.12x29.psf";
        hash = "sha256-RGKYEbALQzGT+h1WKx3dth8q8ffi/uP5xRELph9HLcs=";
      };

      FIRMWARE_PATH = pkgs.OVMF.mergedFirmware;
    };
  };
}
