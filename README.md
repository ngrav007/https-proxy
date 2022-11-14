# http-proxy
Final due data: Wednesday 12/14/2022

## Baseline (due Monday 11/21/2022)
- Support GET method and caching (A1)
- Handle CONNECT method of HTTP (this will allow our proxy to handle HTTPS)
  - See [MDN - CONNECT](https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods/CONNECT)
- Handle multiple clients (A2)
- Error handling (A2+)
- Performance testing and optimizations

## Advance, pick 1+ (due Monday 12/5/2022)
- *Trusted Proxy/SSL
- Proxy Opitmization
- Cooperative Caching 
- *Content Filtering
- Rate Limiting
- Sophisticated Caching Policy
- Basic Search Engine
- Hints 
- Transcoding Support 

## Testing resources 
- HTTP resources
  - http://www.cs.tufts.edu/comp/112/index.html
  - http://www.cs.cmu.edu/~prs/bio.html
  - http://www.cs.cmu.edu/~dga/dga-headshot.jpg

## ARCHITECTURE 


## TODO - for Baseline: handle multiple client
- Add outline for using select() method. 
- Change ChatClient to Client
  - remove extra data members, ex. client id
- Update config file
- Fix Cache to work with LinkedList instead of List 
- Consider adding List_map so we can loop through clients instead of looping until fdmax. 
  - Alternatively, consider looping through client_list nodes instead of looping until fdmax (list nodes are not private, so we can do this)
- Add timeout min update when we are looping through clients
- Halt recognition 
- Error handling
- ask if in http 2.0 do we keep connection with user if we already served them the requested resource.


## Advances - to benchmark later 
- optimization: dynamic key strings for proxy cache. 
