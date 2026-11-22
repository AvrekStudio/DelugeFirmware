# Firmware perso du 16/11/2026 (binaire seul)

`deluge-engine-lfo_last.bin` : dernier firmware perso livré avant la perte du code source (sandbox effacée le
20/11/2026). **Il n'existe pas de code source pour ce binaire** : il est archivé ici tel quel, pour pouvoir le
reflasher et servir de référence.

- Base : community firmware c1.3.0.
- sha256 (début) : f81db4c69ec897cd
- Démarre dans l'émulateur delugemu (vérifié le 22/11/2026).
- Contenu repéré dans le binaire : CHORD MODE (STRUM, BASS, LATCH, écran clavier), CANE (PLUK / BOW / REED / FLUT /
  BELL / DRUM, TIMBRE / COLOR / DAMP), SID (6581 / 8580), EUCLID par NoteRow (2 générateurs, rotate, opérateur),
  GENERATOR, reverbs PLATE + PSX, LEN (longueur de note), indicateur de page « PG x/y ».
