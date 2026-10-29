-- I678j: three albums (Alb1 played 3 times, Alb3 twice, Alb2 once), one playlist "Mix" holding a Alb2 song, one liked song.
INSERT INTO songs(key, source, path, name, artist, album, added) VALUES
('local:a.mod','local','a.mod','a','Ann','Alb1',100),('local:b.mod','local','b.mod','b','Bob','Alb2',200),
('local:c.mod','local','c.mod','c','Cy','Alb3',300);
INSERT INTO albums(name, artist) VALUES('Alb1','Ann'),('Alb2','Bob'),('Alb3','Cy');
INSERT INTO history(key, played_at) VALUES('local:a.mod',1000),('local:a.mod',1100),('local:a.mod',1200),
('local:c.mod',1300),('local:c.mod',1400),('local:b.mod',1500);
INSERT INTO playlists(id, name) VALUES(1,'Mix');
INSERT INTO playlist_entries(playlist_id, pos, key) VALUES(1,0,'local:b.mod');
INSERT INTO liked(key, at) VALUES('local:a.mod',900);
