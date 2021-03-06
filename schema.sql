CREATE TABLE email(id INTEGER PRIMARY KEY, md5 VARCHAR(32) UNIQUE NOT NULL, email VARCHAR(32), bounces INT DEFAULT 0, confirmed TIMESTAMP DEFAULT NULL);
CREATE TABLE faction (id INTEGER PRIMARY KEY, user_id INTEGER REFERENCES user(id), no INTEGER, name VARCHAR(64), game_id INTEGER REFERENCES game(id), race VARCHAR(10), lang CHAR(2));
CREATE TABLE faction_email (faction_id INTEGER REFERENCES faction(id), email_id INTEGER REFERENCES email(id));
CREATE TABLE game (id INTEGER PRIMARY KEY, name VARCHAR(20), last_turn INTEGER);
CREATE TABLE score (turn INTEGER, faction_id INTEGER REFERENCES faction(id), value INTEGER, UNIQUE(turn, faction_id));
CREATE TABLE user(id INTEGER PRIMARY KEY, email_id INTEGER REFERENCES email(id), creation TIMESTAMP DEFAULT CURRENT_TIMESTAMP);
