package main

import (
	"bufio"
	"context"
	"errors"
	"html"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"
)

const hnWebsite = "https://news.ycombinator.com"

var hnUsername = regexp.MustCompile(`^[A-Za-z0-9_-]{1,64}$`)
var errSignIn = errors.New("Sign in to Hacker News in Settings.")

type hnSession struct {
	Username string
	Cookie   string
	Expires  time.Time
}

type hnAccount struct {
	base   string
	dir    string
	client *http.Client
}

func newHNAccount() hnAccount {
	dir := os.Getenv("HNINK_SESSION_DIR")
	if dir == "" {
		if config, err := os.UserConfigDir(); err == nil {
			dir = filepath.Join(config, "hnink")
		}
	}
	return hnAccount{
		base: hnWebsite,
		dir:  dir,
		client: &http.Client{
			Timeout:       6 * time.Second,
			CheckRedirect: func(*http.Request, []*http.Request) error { return http.ErrUseLastResponse },
		},
	}
}

func (a hnAccount) sessionPath() string {
	return filepath.Join(a.dir, "session.json")
}

func (a hnAccount) session() (hnSession, bool) {
	if a.dir == "" {
		return hnSession{}, false
	}
	session, ok := load[hnSession](a.sessionPath())
	ok = ok && hnUsername.MatchString(session.Username) && session.Cookie != "" &&
		(session.Expires.IsZero() || time.Now().Before(session.Expires))
	return session, ok
}

func (a hnAccount) request(ctx context.Context, method, path string, form url.Values, session *hnSession) (*http.Response, error) {
	var body io.Reader
	if form != nil {
		body = strings.NewReader(form.Encode())
	}
	req, err := http.NewRequestWithContext(ctx, method, a.base+path, body)
	if err != nil {
		return nil, errors.New("Couldn’t open Hacker News.")
	}
	if form != nil {
		req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
		req.Header.Set("Origin", a.base)
	}
	if session != nil {
		req.AddCookie(&http.Cookie{Name: "user", Value: session.Cookie})
	}
	resp, err := a.client.Do(req)
	if err != nil {
		return nil, errors.New("Couldn’t reach Hacker News. Check Wi-Fi and try again.")
	}
	return resp, nil
}

func (a hnAccount) login(ctx context.Context, username, password string) error {
	username = strings.TrimSpace(username)
	if !hnUsername.MatchString(username) || password == "" {
		return errors.New("Enter your Hacker News username and password.")
	}
	if a.dir == "" {
		return errors.New("Private storage is unavailable. Couldn’t save the sign-in.")
	}
	if err := os.MkdirAll(a.dir, 0700); err != nil {
		return errors.New("Couldn’t open private storage for the sign-in.")
	}
	if err := os.Chmod(a.dir, 0700); err != nil {
		return errors.New("Couldn’t protect the sign-in on this Kindle.")
	}
	resp, err := a.request(ctx, http.MethodPost, "/login", url.Values{
		"acct": {username}, "pw": {password}, "goto": {"news"},
	}, nil)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode >= 400 && resp.StatusCode != http.StatusUnauthorized {
		return errors.New("Hacker News couldn’t sign you in. Try again later.")
	}
	for _, cookie := range resp.Cookies() {
		if cookie.Name != "user" || cookie.Value == "" || cookie.MaxAge < 0 {
			continue
		}
		expires := cookie.Expires
		if cookie.MaxAge > 0 {
			expires = time.Now().Add(time.Duration(cookie.MaxAge) * time.Second)
		}
		if err := write(a.sessionPath(), hnSession{Username: username, Cookie: cookie.Value, Expires: expires}); err != nil {
			return errors.New("Couldn’t save the sign-in on this Kindle.")
		}
		return nil
	}
	return errors.New("Hacker News didn’t sign you in. Check your username and password.")
}

func (a hnAccount) logout() error {
	if a.dir == "" {
		return nil
	}
	if err := os.Remove(a.sessionPath()); err != nil && !errors.Is(err, os.ErrNotExist) {
		return errors.New("Couldn’t remove the saved sign-in.")
	}
	return nil
}

func (a hnAccount) favoritePath(markup string, id int) (string, bool) {
	for _, match := range anchorTag.FindAllStringSubmatch(markup, -1) {
		ref := htmlURL(match[1] + match[2])
		if ref == nil || (ref.IsAbs() && ref.Scheme+"://"+ref.Host != a.base) ||
			(!ref.IsAbs() && ref.Host != "") || ref.User != nil {
			continue
		}
		path := strings.TrimPrefix(ref.Path, "/")
		if path != "fave" && path != "unfave" {
			continue
		}
		query := ref.Query()
		if query.Get("id") != num(id) || query.Get("auth") == "" {
			continue
		}
		return "/fave?" + ref.RawQuery, path == "unfave" || query.Has("un")
	}
	return "", false
}

func htmlURL(address string) *url.URL {
	address = html.UnescapeString(address)
	if urlChars.MatchString(address) {
		return nil
	}
	ref, err := url.Parse(address)
	if err != nil {
		return nil
	}
	return ref
}

func (a hnAccount) itemPage(ctx context.Context, id int, session *hnSession) (string, error) {
	resp, err := a.request(ctx, http.MethodGet, "/item?id="+num(id), nil, session)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return "", errors.New("Couldn’t open the story on Hacker News.")
	}
	text, err := io.ReadAll(io.LimitReader(resp.Body, 512<<10))
	return string(text), err
}

func (a hnAccount) addFavorite(ctx context.Context, id int) error {
	session, ok := a.session()
	if !ok {
		return errSignIn
	}
	page, err := a.itemPage(ctx, id, &session)
	if err != nil {
		return err
	}
	path, already := a.favoritePath(page, id)
	if already {
		a.markFavorite(session, id)
		return nil
	}
	if path == "" {
		if strings.Contains(page, `href="login?goto=`) {
			return errSignIn
		}
		return errors.New("Hacker News didn’t offer Favorites for this story.")
	}
	resp, err := a.request(ctx, http.MethodGet, path, nil, &session)
	if err != nil {
		return err
	}
	resp.Body.Close()
	if resp.StatusCode >= 400 {
		return errors.New("Hacker News couldn’t add this story to Favorites.")
	}
	page, err = a.itemPage(ctx, id, &session)
	if err != nil {
		return err
	}
	if _, favorite := a.favoritePath(page, id); !favorite {
		return errors.New("Hacker News didn’t confirm the favorite. Try again.")
	}
	a.markFavorite(session, id)
	return nil
}

func (a hnAccount) favoriteMarker(session hnSession, id int) string {
	return filepath.Join(a.dir, "favorites", session.Username, num(id)+".json")
}

func (a hnAccount) markFavorite(session hnSession, id int) {
	_ = write(a.favoriteMarker(session, id), true)
}

func readCredentials(input io.Reader) (string, string) {
	scanner := bufio.NewScanner(io.LimitReader(input, 8192))
	scanner.Buffer(make([]byte, 512), 4096)
	if !scanner.Scan() {
		return "", ""
	}
	username := scanner.Text()
	if !scanner.Scan() {
		return "", ""
	}
	return username, scanner.Text()
}

func runAccount(ctx context.Context, a hnAccount, out io.Writer, input io.Reader, args []string) bool {
	var err error
	switch args[0] {
	case "account":
	case "login":
		user, password := readCredentials(input)
		err = a.login(ctx, user, password)
	case "logout":
		err = a.logout()
	case "favorite", "favorite-status":
		var id int
		if len(args) > 1 {
			id, _ = strconv.Atoi(args[1])
		}
		if args[0] == "favorite-status" {
			if session, ok := a.session(); ok && id > 0 {
				if saved, _ := load[bool](a.favoriteMarker(session, id)); saved {
					record(out, "favorite", "true")
				}
			}
			return true
		}
		if id <= 0 {
			err = errors.New("No story selected.")
		} else {
			err = a.addFavorite(ctx, id)
		}
		if err == nil {
			record(out, "favorite", "Added to Hacker News Favorites.")
			return true
		}
	default:
		return false
	}
	if err != nil {
		if errors.Is(err, errSignIn) {
			_ = a.logout()
			record(out, "expired", err.Error())
		} else {
			record(out, "error", err.Error())
		}
	} else {
		session, _ := a.session()
		record(out, "account", session.Username)
	}
	return true
}
