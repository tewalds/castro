require 'net/http'
require 'yaml'

#fix rbuf_fill error
#http://www.ruby-forum.com/topic/105212
#module Net
#  class BufferedIO
#    def rbuf_fill
#      timeout(@read_timeout,ProtocolError) {
#      @rbuf << @io.sysread(BUFSIZE)
#      }
#    end
#  end
#end

class String
	def red
		"\e[31m#{self}\e[0m"
	end
	def red_back
		"\e[41m#{self}\e[0m"
	end
	def green
		"\e[32m#{self}\e[0m"
	end  
	def blue
		"\e[34m#{self}\e[0m"
	end
	def yellow
		"\e[33m#{self}\e[0m"
	end
end

class Logger
	def log(msg)
		puts((('[' + Time::now.strftime('%d-%m-%y %H:%M:%S') + '] ').yellow) + msg)
	end
end

class LittleGolemInterface
	def initialize (loginname,psw,boss_id)
		@login,@psw,@boss_id=loginname,psw,boss_id
		@http = Net::HTTP.new('www.littlegolem.net')
		@logger=Logger.new
	end
	def get_game(gid)
		path="/servlet/sgf/#{gid}/game.hgf"
		resp = @http.get(path, @headers)
		return (resp.code == '200' ? resp.body : nil)
	end
	def get_invitations
		path='/jsp/invitation/index.jsp'
		resp = @http.get(path, @headers)
		return (resp.code == '200' ? resp.body : nil)
	end
	def send_message(pid,title,msg)
		path="/jsp/message/new.jsp"
		resp = @http.post(path,"messagetitle=#{title}&message=#{msg}&plto=#{pid}", @headers)
		return (resp.code == '200' ? resp.body : nil)
	end
	def post_move(gid, mv, chat = '')
		chat.sub!('+',' leads with ')
		path="/jsp/game/game.jsp?sendgame=#{gid}&sendmove=#{mv}"
		resp = @http.post(path, "message=#{chat}", @headers)
		if resp.code!='200'
			logout
			login
			resp = @http.post(path, "message=#{chat}", @headers)
		end
		return (resp.code == '200' ? resp.body : nil)
	end
	def reply_invitation(inv_id,answer)
		path="/Invitation.action?#{answer}=&invid=#{inv_id}"
		resp = @http.get(path, @headers)
		return (resp.code == '200' ? resp.body : nil)
	end
	def log(msg)
		@logger.log(msg)
	end
	def logout
		path="/jsp/login/logoff.jsp"
		resp = @http.get(path, @headers)
		@headers = nil
		return (resp.code == '200' ? resp.body : nil)
	end
	def login
		path='/jsp/login/index.jsp'
		resp = @http.get(path, nil)
		@headers = {'Cookie' => resp['set-cookie'] }#, 'Content-Type' => 'using application/x-www-form-urlencoded' }

		data = "login=#{@login}&password=#{@psw}"
		resp = @http.post(path, data, @headers)

		return (resp.code == '200' ? resp.body : nil)
	end
	def get_gamesheet
		path='/jsp/game/index.jsp'
		resp = @http.get(path, @headers)
		return (resp.code == '200' ? resp.body : nil)
	end
	def get_my_games
		path="/get_xml.jsp"
		resp = @http.post(path, "login=#{@login}&password=#{@psw}")
		return (resp.code == '200' ? resp.body : nil)
	end
	def get_my_turn_games
		if self.login 
			if (gamesheet = get_gamesheet)
				if !(gamesheet =~  /On Move/)
					return gamesheet.slice(/On Move.*Opponent's Move/m).scan(/gid=(\d+)?/).flatten
				end  
			end 
		else
			self.log("Could not log in, #{@sleep}s sleep".red_back)
		end
		[]
	end
	def parse
		if !self.login
			self.log('login failed'.red_back)
			sleep(600)
			return false;
		end

		if (gamesheet = get_gamesheet)
			#check invitations
			if gamesheet =~ /New invitations:/
				if invites = get_invitations
					#a = invites.slice(/Your decision.*?Confirm selection/m).scan(/<td>(.*?)<\/td>/m).flatten
					a = invites.slice(/Your decision.*?table>/m).scan(/<td>(.*?)<\/td>/m).flatten
					opponent = a[1]
					gametype = a[2]
					if gametype =~ @supported_gametypes
						answer='accept'
					else
						answer='refuse'
					end
					self.send_message(@boss_id,"New invitation","#{answer} #{gametype} from #{opponent}")
					self.log("#{answer} #{gametype} from #{opponent}".green)
					inv_id = a[5].scan(/invid=(\d*)?/m)[0]
					reply_invitation(inv_id, answer)
				end
			end

			#play a move
			if (gamesheet =~  /On Move/)
				gameids=gamesheet.slice(/On Move.*Opponent's Move/m).scan(/gid=(\d+)?/).flatten
				parse_make_moves(gameids)
				return true;
			else
				self.log("No games found where it's my turn, sleep")
				return false
			end
		end
	end
end

module HavannahCoords
#Assume all coords are in yx format

#GA - Hgui/GTP
#       1  2  3  4  5  6  7
#A      +  o  o  +
#B      o  1  .  .  o
#C      o  .  .  .  .  o
#D      +  .  .  .  .  .  +
#E         o  .  .  .  .  o
#F            o  .  2  .  o
#G               +  o  o  +
#White starts

#LG - LG web interface
#       r  q  p  o  n  m  l
#l      +  o  o  +
#m      o  1  .  .  o
#n      o  .  .  .  .  o
#o      +  .  .  .  .  .  +
#p         o  .  .  .  .  o
#q            o  .  2  .  o
#r               +  o  o  +
#Black starts

#HGF - LG hgf files
#       1  2  3  4  5  6  7
#A      +  o  o  +
#B      o  1  .  .  o
#C      o  .  .  .  .  o
#D      +  .  .  .  .  .  +
#E      o  .  .  .  .  o
#F      o  .  2  .  o
#G      +  o  o  +
#White starts

	def coord_GA2LG(c,size)
		#centered at oo
		return c if c == 'swap'
		c.upcase!

		y = ('o'[0] - size + (c[0]-64)   ).chr
		x = ('o'[0] + size - c[1..2].to_i).chr
		return y+x
	end
	def coord_GA2HGF(c,size)
		return c if c == 'swap'
		c.upcase!
		y = c[0].chr
		x = c[1..2].to_i
		x -= (y[0]-64)-size if size < (y[0]-64)
		return y + x.to_s
	end
	def coord_HGF2GA(c,size)
		return c if c == 'swap'
		c.upcase!
		y = c[0].chr
		x = c[1..2].to_i
		x += (y[0]-64)-size if size < (y[0]-64)
		return y + x.to_s
	end
	def test_coords
		coord_GA2LG('D4',4) == 'oo' or raise "GA2LG not centered"
		coord_GA2LG('B2',4) == 'mq' or raise "GA2LG fails on 1"
		coord_GA2LG('F5',4) == 'qn' or raise "GA2LG fails on 2"

		coord_GA2HGF('D4',4) == 'D4' or raise "GA2HGF not centered"
		coord_GA2HGF('B2',4) == 'B2' or raise "GA2HGF fails on 1"
		coord_GA2HGF('F5',4) == 'F3' or raise "GA2HGF fails on 2"

		coord_HGF2GA('D4',4) == 'D4' or raise "HGF2GA not centered"
		coord_HGF2GA('B2',4) == 'B2' or raise "HGF2GA fails on 1"
		coord_HGF2GA('F3',4) == 'F5' or raise "HGF2GA fails on 2"

#		coord_GA2LG('d4',4) == 'blah' or raise "bad HavannahCoord test"
	end
end

