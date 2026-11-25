package main

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"html"
	"io"
	"net"
	"net/http"
	"net/netip"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"
	"unicode/utf8"
)

const apiBase = "https://hacker-news.firebaseio.com/v0/"
const feedLimit = 60

type item struct {
	ID          int       `json:"id"`
	Title       string    `json:"title"`
	Text        string    `json:"text"`
	Links       []string  `json:"_links"`
	URL         string    `json:"url"`
	By          string    `json:"by"`
	Time        int64     `json:"time"`
	Score       int       `json:"score"`
	Descendants int       `json:"descendants"`
	Kids        []int     `json:"kids"`
	Deleted     bool      `json:"deleted"`
	Dead        bool      `json:"dead"`
	Fetched     time.Time `json:"_fetched"`
}

type snapshot struct {
	IDs     []int
	Updated time.Time
}

type app struct {
	dir    string
	api    string
	client *http.Client
}

var feeds = map[string]bool{"top": true, "new": true, "best": true, "ask": true, "show": true, "jobs": true}

var tag = regexp.MustCompile(`(?s)<[!/?a-zA-Z][^>]*>`)
var blocks = regexp.MustCompile(`(?i)</?(p|br|li|pre|h[1-6]|blockquote|div|section)\b[^>]*>`)
var spaces = regexp.MustCompile(`[\t\r ]+`)
var emptyLines = regexp.MustCompile(`\n[ \n]*\n`)
var anchorTag = regexp.MustCompile(`(?is)<a\b[^>]*?\s+href\s*=\s*(?:"([^"]*)"|'([^']*)')`)

func commentLinks(markup string) []string {
	links := make([]string, 0)
	seen := make(map[string]bool)
	base := &url.URL{Scheme: "https", Host: "news.ycombinator.com"}
	for _, match := range anchorTag.FindAllStringSubmatch(markup, 24) {
		address := html.UnescapeString(match[1] + match[2])
		if address == "" || urlChars.MatchString(address) {
			continue
		}
		resolved, err := base.Parse(address)
		if err != nil {
			continue
		}
		address = resolved.String()
		if _, err := webURL(address); err == nil && !seen[address] {
			links = append(links, address)
			seen[address] = true
		}
	}
	return links
}

var urlChars = regexp.MustCompile(`[\x00-\x20\x7f\\]`)

func plain(markup string) string {
	text := blocks.ReplaceAllString(markup, "\n\n")
	text = html.UnescapeString(tag.ReplaceAllString(text, ""))
	text = strings.Map(func(r rune) rune {
		if r == '\n' || r == '\t' || r >= 32 && r != 127 {
			return r
		}
		return -1
	}, text)
	return strings.TrimSpace(emptyLines.ReplaceAllString(spaces.ReplaceAllString(text, " "), "\n\n"))
}

func oneLine(text string) string { return strings.Join(strings.Fields(text), " ") }

func shorten(text string, limit int) string {
	if len(text) <= limit {
		return text
	}
	for !utf8.RuneStart(text[limit]) {
		limit--
	}
	return text[:limit] + "…"
}

func webURL(address string) (*url.URL, error) {
	u, err := url.Parse(address)
	if err != nil || len(address) > 4096 || urlChars.MatchString(address) || u.User != nil ||
		(u.Scheme != "https" && u.Scheme != "http") || u.Hostname() == "" {
		return nil, errors.New("invalid web address")
	}
	return u, nil
}

func browserCommand(platform string, address *url.URL) (string, []string) {
	if platform == "darwin" {
		return "open", []string{address.String()}
	}
	query := url.Values{"url": {address.String()}}.Encode()
	return "lipc-set-prop", []string{"com.lab126.appmgrd", "start", "app://com.lab126.browser?" + query}
}

func (a app) publisherURL(id int) (*url.URL, error) {
	i, ok := a.getItem(id)
	if !ok || i.Deleted || i.Dead {
		return nil, errors.New("story is unavailable")
	}
	return webURL(i.URL)
}

func openPublisher(ctx context.Context, address *url.URL) error {
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()
	program, args := browserCommand(runtime.GOOS, address)
	return exec.CommandContext(ctx, program, args...).Run()
}

func newArticleClient() *http.Client {
	tr := &http.Transport{
		DialContext: func(ctx context.Context, network, address string) (net.Conn, error) {
			host, port, err := net.SplitHostPort(address)
			if err != nil || (port != "443" && port != "80") {
				return nil, errors.New("unsupported publisher port")
			}
			answers, err := net.DefaultResolver.LookupNetIP(ctx, "ip", host)
			if err != nil {
				return nil, err
			}
			dialer := net.Dialer{Timeout: 3 * time.Second}
			for _, answer := range answers {
				ip := answer.Unmap()
				if !ip.IsGlobalUnicast() || ip.IsPrivate() || carrierNAT.Contains(ip) {
					continue
				}
				conn, dialErr := dialer.DialContext(ctx, network, net.JoinHostPort(ip.String(), port))
				if dialErr == nil {
					return conn, nil
				}
			}
			return nil, errors.New("publisher is unreachable")
		},
		TLSHandshakeTimeout: 4 * time.Second,
	}
	return &http.Client{
		Transport: tr, Timeout: 12 * time.Second,
		CheckRedirect: func(req *http.Request, via []*http.Request) error {
			if len(via) >= 5 || (via[len(via)-1].URL.Scheme == "https" && req.URL.Scheme != "https") {
				return errors.New("unsupported publisher redirect")
			}
			_, err := webURL(req.URL.String())
			return err
		},
	}
}

var carrierNAT = netip.MustParsePrefix("100.64.0.0/10")

func (a app) path(group, key string) string {
	return filepath.Join(a.dir, group, key+".json")
}

func load[T any](path string) (T, bool) {
	var data T
	f, err := os.Open(path)
	if err != nil {
		return data, false
	}
	defer f.Close()
	err = json.NewDecoder(io.LimitReader(f, 1<<20)).Decode(&data)
	return data, err == nil
}

func write(path string, data any) error {
	dir := filepath.Dir(path)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}
	f, err := os.CreateTemp(dir, ".hnink-*")
	if err != nil {
		return err
	}
	defer os.Remove(f.Name())
	if err = json.NewEncoder(f).Encode(data); err != nil {
		f.Close()
		return err
	}
	if err := f.Close(); err != nil {
		return err
	}
	return os.Rename(f.Name(), path)
}

func (a app) getJSON(ctx context.Context, path string, into any) error {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, a.api+path+".json", nil)
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("User-Agent", "HNink/"+version)
	resp, err := a.client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("Hacker News returned HTTP %d", resp.StatusCode)
	}
	return json.NewDecoder(io.LimitReader(resp.Body, 512<<10)).Decode(into)
}

func (a app) getItem(id int) (item, bool) {
	return load[item](a.path("items", strconv.Itoa(id)))
}

func (a app) fetchItem(ctx context.Context, id int, fresh bool) bool {
	existing, have := a.getItem(id)
	if have && !fresh && (existing.Links != nil || !strings.Contains(existing.Text, "http")) {
		return true
	}
	var i *item
	if err := a.getJSON(ctx, "item/"+strconv.Itoa(id), &i); err != nil {
		return have
	}
	if i == nil {
		i = &item{ID: id, Deleted: true}
	}
	if i.ID != id {
		return have
	}
	i.Title = shorten(oneLine(plain(i.Title)), 1024)
	i.Links = commentLinks(i.Text)
	i.Text = plain(i.Text)
	i.By = shorten(oneLine(i.By), 96)
	if _, err := webURL(i.URL); err != nil {
		i.URL = ""
	}
	i.Kids = uniqueIDs(i.Kids, 2000)
	i.Fetched = time.Now()
	return write(a.path("items", strconv.Itoa(id)), i) == nil || have
}

func uniqueIDs(source []int, limit int) []int {
	result := make([]int, 0, min(len(source), limit))
	seen := make(map[int]bool)
	for _, id := range source {
		if id > 0 && !seen[id] {
			result = append(result, id)
			seen[id] = true
			if len(result) == limit {
				break
			}
		}
	}
	return result
}

func (a app) fetchItems(ctx context.Context, ids []int, fresh bool) int {
	var wg sync.WaitGroup
	jobs := make(chan int, len(ids))
	results := make(chan bool, len(ids))
	for _, id := range ids {
		jobs <- id
	}
	close(jobs)
	for n := 0; n < min(5, len(ids)); n++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for id := range jobs {
				if ctx.Err() != nil {
					return
				}
				results <- a.fetchItem(ctx, id, fresh)
			}
		}()
	}
	wg.Wait()
	close(results)
	have := 0
	for ok := range results {
		if ok {
			have++
		}
	}
	return have
}

func (a app) feed(name string) snapshot {
	if name == "saved" {
		entries, _ := os.ReadDir(filepath.Join(a.dir, "saved"))
		type savedEntry struct {
			ID int
			At time.Time
		}
		saved := make([]savedEntry, 0, len(entries))
		for _, entry := range entries {
			id, err := strconv.Atoi(strings.TrimSuffix(entry.Name(), ".json"))
			if err == nil && id > 0 {
				if at, ok := load[time.Time](a.path("saved", strconv.Itoa(id))); ok {
					saved = append(saved, savedEntry{id, at})
				}
			}
		}
		sort.Slice(saved, func(i, j int) bool { return saved[i].At.After(saved[j].At) })
		s := snapshot{}
		for _, entry := range saved {
			s.IDs = append(s.IDs, entry.ID)
		}
		return s
	}
	s, _ := load[snapshot](a.path("feeds", name))
	return s
}

func pageOf(ids []int, page, size int) ([]int, int, int) {
	pages := max(1, (len(ids)+size-1)/size)
	page = max(1, min(page, pages))
	first := min(len(ids), (page-1)*size)
	return ids[first:min(len(ids), first+size)], page, pages
}

func anchorPage(ids []int, id, fallback, size int) int {
	if id > 0 {
		for index, candidate := range ids {
			if candidate == id {
				return index/size + 1
			}
		}
	}
	return fallback
}

func (a app) sync(ctx context.Context, name string, page, size, anchor int) error {
	if !feeds[name] {
		return errors.New("unknown feed")
	}
	var ids []int
	endpoint := name + "stories"
	if name == "jobs" {
		endpoint = "jobstories"
	}
	if err := a.getJSON(ctx, endpoint, &ids); err != nil {
		return err
	}
	ids = uniqueIDs(ids, feedLimit)
	page = anchorPage(ids, anchor, page, size)
	visible, _, _ := pageOf(ids, page, size)
	nearby, next, _ := pageOf(ids, page+1, size)
	batch := append([]int(nil), visible...)
	if next > page {
		batch = append(batch, nearby...)
	}
	a.fetchItems(ctx, batch, true)
	if len(visible) > 0 && !a.anyCached(visible) {
		return errors.New("story details unavailable")
	}
	return write(a.path("feeds", name), snapshot{IDs: ids, Updated: time.Now()})
}

func (a app) anyCached(ids []int) bool {
	for _, id := range ids {
		if _, ok := a.getItem(id); ok {
			return true
		}
	}
	return false
}

func record(w io.Writer, kind string, values ...string) {
	fmt.Fprint(w, kind)
	for _, value := range values {
		fmt.Fprint(w, "\t", base64.StdEncoding.EncodeToString([]byte(value)))
	}
	fmt.Fprintln(w)
}

func num(n int) string { return strconv.Itoa(n) }

func host(address string) string {
	if u, err := webURL(address); err == nil {
		return strings.TrimPrefix(u.Hostname(), "www.")
	}
	return ""
}

func age(seconds int64) string {
	if seconds <= 0 {
		return ""
	}
	d := time.Since(time.Unix(seconds, 0))
	switch {
	case d < time.Minute:
		return "now"
	case d < time.Hour:
		return fmt.Sprintf("%dm", d/time.Minute)
	case d < 24*time.Hour:
		return fmt.Sprintf("%dh", d/time.Hour)
	default:
		return fmt.Sprintf("%dd", d/(24*time.Hour))
	}
}

func joinMeta(fields ...string) string {
	var result []string
	for _, field := range fields {
		if field != "" {
			result = append(result, field)
		}
	}
	return strings.Join(result, "  ·  ")
}

func (a app) view(w io.Writer, name string, page, size, anchor int) {
	s := a.feed(name)
	ids, page, pages := pageOf(s.IDs, anchorPage(s.IDs, anchor, page, size), size)
	stamp := ""
	if !s.Updated.IsZero() {
		stamp = s.Updated.Local().Format("Jan 2, 15:04")
	}
	record(w, "page", num(page), num(pages), stamp, "")
	for n, id := range ids {
		i, ok := a.getItem(id)
		rank := num((page-1)*size + n + 1)
		if !ok {
			record(w, "row", num(id), rank, "Story not cached", "Refresh to load this page.", "")
			continue
		}
		if i.Deleted || i.Dead {
			record(w, "row", num(id), rank, "Story unavailable", "", "")
			continue
		}
		_, read := load[bool](a.path("read", num(id)))
		marker := ""
		if read {
			marker = "read"
		}
		comments := num(i.Descendants) + " comments"
		if i.Descendants == 1 {
			comments = "1 comment"
		}
		record(w, "row", num(id), rank, i.Title,
			joinMeta(host(i.URL), num(i.Score)+" points", comments, age(i.Time)), marker)
	}
}

func (a app) detail(w io.Writer, id int) error {
	i, ok := a.getItem(id)
	if !ok || i.Deleted || i.Dead || i.Title == "" {
		return errors.New("story is unavailable")
	}
	_, saved := load[time.Time](a.path("saved", num(id)))
	record(w, "story", num(id), i.Title, joinMeta(i.By, host(i.URL), num(i.Score)+" points", age(i.Time)), i.Text, i.URL,
		num(i.Descendants), strconv.FormatBool(saved))
	return nil
}

const inlineCommentDepth = 3

func writeComment(w io.Writer, kind string, i item, depth int) {
	body := i.Text
	if i.Deleted || i.Dead {
		body = "[Comment removed]"
	}
	links := strings.Join(i.Links, "\n")
	if i.Deleted || i.Dead {
		links = ""
	}
	record(w, kind, num(i.ID), i.By, body, age(i.Time), num(len(i.Kids)), num(depth), links)
}

func (a app) comments(w io.Writer, parent, page, size int) error {
	p, ok := a.getItem(parent)
	if !ok {
		return errors.New("discussion is unavailable")
	}
	ids := p.Kids
	pages := 1
	if size > 0 {
		ids, page, pages = pageOf(ids, page, size)
	}
	record(w, "page", num(page), num(pages), "", p.Title)
	if p.Title == "" {
		writeComment(w, "context", p, 0)
	}
	seen := map[int]bool{parent: true}
	var show func([]int, int)
	show = func(ids []int, depth int) {
		for _, id := range ids {
			if seen[id] {
				continue
			}
			seen[id] = true
			i, have := a.getItem(id)
			if !have {
				record(w, "missing", num(id), "", "Comment not cached. Refresh to load it.", "", "0", num(depth))
				continue
			}
			writeComment(w, "comment", i, depth)
			if depth < inlineCommentDepth {
				show(i.Kids, depth+1)
			}
		}
	}
	show(ids, 1)
	return nil
}

func (a app) fetchComments(ctx context.Context, parentID, page, size int, fresh bool) error {
	a.fetchItem(ctx, parentID, fresh)
	parent, ok := a.getItem(parentID)
	if !ok {
		return errors.New("discussion is unavailable")
	}
	level, _, _ := pageOf(parent.Kids, page, size)
	return a.fetchCommentTree(ctx, parentID, level, fresh)
}

func (a app) fetchCommentTree(ctx context.Context, parentID int, level []int, fresh bool) error {
	seen := map[int]bool{parentID: true}
	complete := true
	for depth := 1; depth <= inlineCommentDepth && len(level) > 0 && ctx.Err() == nil; depth++ {
		ids := make([]int, 0, len(level))
		for _, id := range level {
			if !seen[id] {
				ids = append(ids, id)
				seen[id] = true
			}
		}
		if a.fetchItems(ctx, ids, fresh && depth == 1) < len(ids) {
			complete = false
		}
		if depth == inlineCommentDepth {
			break
		}
		level = nil
		for _, id := range ids {
			if comment, have := a.getItem(id); have {
				level = append(level, comment.Kids...)
			}
		}
	}
	if !complete {
		return errors.New("some comments could not be loaded")
	}
	return nil
}

// discussion emits the cache immediately, then the updated tree in a few batches.
func (a app) discussion(ctx context.Context, w io.Writer, id int, fresh bool) error {
	publish := func() error {
		if err := a.comments(w, id, 1, 0); err != nil {
			return err
		}
		_, err := fmt.Fprintln(w, ".")
		return err
	}
	parent, cached := a.getItem(id)
	freshness := a.path("discussion", num(id))
	if cached {
		if parent.Title != "" {
			_ = write(a.path("read", num(id)), true)
		}
		if err := publish(); err != nil {
			return err
		}
		if loaded, ok := load[time.Time](freshness); !fresh && ok &&
			time.Since(loaded) < 5*time.Minute && !parent.Fetched.After(loaded) {
			return nil
		}
	}
	update := fresh || !cached || time.Since(parent.Fetched) > 5*time.Minute
	before := parent.Fetched
	if update && !a.fetchItem(ctx, id, true) {
		return errors.New("discussion is unavailable")
	}
	parent, ok := a.getItem(id)
	if !ok {
		return errors.New("discussion is unavailable")
	}
	complete := !update || parent.Fetched.After(before)
	const batchSize = 10
	last := time.Now()
	for start := 0; start < len(parent.Kids) && ctx.Err() == nil; start += batchSize {
		end := min(start+batchSize, len(parent.Kids))
		if err := a.fetchCommentTree(ctx, id, parent.Kids[start:end], fresh); err != nil {
			complete = false
		}
		if end < len(parent.Kids) && (start == 0 || time.Since(last) >= 3*time.Second) {
			if err := publish(); err != nil {
				return err
			}
			last = time.Now()
		}
	}
	if complete && ctx.Err() == nil {
		_ = write(freshness, time.Now())
	}
	return publish()
}

var noisy = regexp.MustCompile(`(?is)<(?:script|style|noscript|nav|footer|aside|form|svg|template|iframe)\b[^>]*>.*?</(?:script|style|noscript|nav|footer|aside|form|svg|template|iframe)\s*>`)
var htmlComments = regexp.MustCompile(`(?s)<!--.*?-->`)
var articleCandidates = []*regexp.Regexp{
	regexp.MustCompile(`(?is)<article\b[^>]*>(.*?)</article\s*>`),
	regexp.MustCompile(`(?is)<main\b[^>]*>(.*?)</main\s*>`),
	regexp.MustCompile(`(?is)<body\b[^>]*>(.*?)</body\s*>`),
}

func articleText(markup string) string {
	clean := noisy.ReplaceAllString(htmlComments.ReplaceAllString(markup, ""), "")
	for _, candidate := range articleCandidates {
		best := ""
		for _, match := range candidate.FindAllStringSubmatch(clean, -1) {
			text := plain(match[1])
			if len(text) > len(best) {
				best = text
			}
		}
		if len(best) >= 140 {
			return shorten(best, 80000)
		}
	}
	return ""
}

func (a app) article(ctx context.Context, id int, cachedOnly bool) (string, error) {
	path := a.path("articles", num(id))
	if text, ok := load[string](path); ok && text != "" {
		return text, nil
	}
	if cachedOnly {
		return "", errors.New("article has not been downloaded")
	}
	i, ok := a.getItem(id)
	if !ok || i.URL == "" {
		return "", errors.New("this story has no publisher link")
	}
	text, err := readablePage(ctx, i.URL)
	if err != nil {
		return "", err
	}
	if err := write(path, text); err != nil {
		return "", err
	}
	return text, nil
}

// projectURL is the page Settings links to. GitHub renders the README inside <article>,
// so the in-app reader can show it without leaving HNink.
const projectURL = "https://github.com/AlirezaXdehghan/HNink"

// about returns the project page text, refreshed at most once a week.
func (a app) about(ctx context.Context, cachedOnly bool) (string, error) {
	path := a.path("articles", "about")
	if info, err := os.Stat(path); err == nil && time.Since(info.ModTime()) < 7*24*time.Hour {
		if text, ok := load[string](path); ok && text != "" {
			return text, nil
		}
	}
	if cachedOnly {
		return "", errors.New("project page has not been downloaded")
	}
	text, err := readablePage(ctx, projectURL)
	if err != nil {
		return "", err
	}
	if err := write(path, text); err != nil {
		return "", err
	}
	return text, nil
}

func readablePage(ctx context.Context, address string) (string, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, address, nil)
	if err != nil {
		return "", err
	}
	req.Header.Set("Accept", "text/html, text/plain;q=0.9")
	req.Header.Set("User-Agent", "HNink/"+version)
	res, err := newArticleClient().Do(req)
	if err != nil {
		return "", errors.New("publisher could not be reached")
	}
	defer res.Body.Close()
	if res.StatusCode != 200 {
		return "", fmt.Errorf("publisher returned HTTP %d", res.StatusCode)
	}
	const maxPage = 2 << 20
	body, err := io.ReadAll(io.LimitReader(res.Body, maxPage+1))
	if err != nil || len(body) > maxPage {
		return "", errors.New("publisher page is too large or incomplete")
	}
	mime := strings.ToLower(res.Header.Get("Content-Type"))
	text := ""
	switch {
	case strings.Contains(mime, "text/plain"):
		text = shorten(strings.ReplaceAll(strings.ToValidUTF8(string(body), "�"), "\x00", ""), 80000)
	case mime == "" || strings.Contains(mime, "html"):
		text = articleText(strings.ToValidUTF8(string(body), "�"))
	}
	if strings.TrimSpace(text) == "" {
		return "", errors.New("no readable article text found")
	}
	return text, nil
}

func run(ctx context.Context, a app, w io.Writer, args []string) error {
	at := func(index int) string {
		if index < len(args) {
			return args[index]
		}
		return ""
	}
	number := func(index, fallback int) int {
		n, err := strconv.Atoi(at(index))
		if err != nil || n <= 0 || n > 2147483647 {
			return fallback
		}
		return n
	}
	page, size, id := number(2, 1), min(feedLimit, number(3, 6)), number(1, 0)
	arg := at(1)
	switch at(0) {
	case "open-url":
		address, err := webURL(arg)
		if err != nil {
			return err
		}
		return openPublisher(ctx, address)
	case "view", "sync", "hydrate":
		if !feeds[arg] && arg != "saved" {
			return errors.New("unknown feed")
		}
		anchor := number(4, 0)
		if at(0) == "sync" {
			return a.sync(ctx, arg, page, size, anchor)
		}
		if at(0) == "hydrate" {
			ids, _, _ := pageOf(a.feed(arg).IDs, page, size)
			if a.fetchItems(ctx, ids, false) == 0 && len(ids) > 0 {
				return errors.New("story details are unavailable")
			}
			return nil
		}
		a.view(w, arg, page, size, anchor)
	case "about", "cached-about":
		text, err := a.about(ctx, at(0) == "cached-about")
		if err != nil {
			return err
		}
		record(w, "article", text)
	case "detail", "read", "bookmark", "comments", "fetch-comments", "discussion", "article", "cached-article", "publisher-link", "open-publisher":
		if id == 0 {
			return errors.New("invalid story or comment")
		}
		switch at(0) {
		case "publisher-link", "open-publisher":
			address, err := a.publisherURL(id)
			if err != nil {
				return err
			}
			if at(0) == "open-publisher" {
				return openPublisher(ctx, address)
			}
			_, err = io.WriteString(w, address.String())
			return err
		case "detail":
			return a.detail(w, id)
		case "read":
			return write(a.path("read", num(id)), true)
		case "bookmark":
			path := a.path("saved", num(id))
			if _, ok := load[time.Time](path); ok {
				return os.Remove(path)
			}
			return write(path, time.Now())
		case "discussion":
			return a.discussion(ctx, w, id, at(2) == "fresh")
		case "comments":
			return a.comments(w, id, page, min(12, size))
		case "fetch-comments":
			return a.fetchComments(ctx, id, page, min(12, size), at(4) == "fresh")
		case "article", "cached-article":
			text, err := a.article(ctx, id, at(0) == "cached-article")
			if err != nil {
				return err
			}
			record(w, "article", text)
		}
	default:
		return errors.New("unknown command")
	}
	return nil
}

// version is set at build time with -ldflags "-X main.version=…".
var version = "dev"

func main() {
	if len(os.Args) == 2 && os.Args[1] == "--version" {
		// The launcher runs this first, so a firmware that can't run Go reports it on screen.
		fmt.Println("hnink-data", version)
		return
	}
	if len(os.Args) < 3 {
		fmt.Fprintln(os.Stderr, "Usage: hnink-data CACHE-DIR COMMAND [ARGS]")
		os.Exit(2)
	}
	timeout := 16 * time.Second
	if os.Args[2] == "discussion" {
		timeout = 35 * time.Second
	}
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	a := app{dir: os.Args[1], api: apiBase, client: &http.Client{Timeout: 6 * time.Second}}
	if runAccount(ctx, newHNAccount(), os.Stdout, os.Stdin, os.Args[2:]) {
		return
	}
	if err := run(ctx, a, os.Stdout, os.Args[2:]); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
